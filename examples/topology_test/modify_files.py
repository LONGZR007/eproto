#!/usr/bin/env python3
import os
import re

def modify_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    original_content = content
    
    # Step 1: Add semaphore includes and variables
    if '#include <semaphore.h>' not in content:
        content = content.replace('#include <sys/time.h>', '#include <sys/time.h>\n#include <semaphore.h>')
    
    # Add semaphore variables after the static int g_needs_reply = 0; line
    semaphore_vars = '''static sem_t g_semaphore;
static int g_semaphore_initialized = 0;'''
    
    if 'static sem_t g_semaphore;' not in content:
        content = content.replace('static int g_needs_reply = 0;', 
                               'static int g_needs_reply = 0;\n\n' + semaphore_vars)
    
    # Step 2: Replace mock_signal_wait function
    old_signal_wait = '''static eproto_signal_result_t mock_signal_wait\\(uint32_t timeout_ms\\) \\{
    usleep\\(timeout_ms \\* 1000\\);
    return EPROTO_SIGNAL_DATA;
\\}'''
    
    new_signal_wait = '''static eproto_signal_result_t mock_signal_wait(uint32_t timestamp) {
    if (!g_semaphore_initialized) {
        if (sem_init(&g_semaphore, 0, 0) != 0) {
            printf("Failed to initialize semaphore\\n");
            return EPROTO_SIGNAL_TIMEOUT;
        }
        g_semaphore_initialized = 1;
    }
    
    uint32_t current_time = mock_get_timestamp();
    uint32_t timeout_ms = 0;
    if (timestamp > current_time) {
        timeout_ms = timestamp - current_time;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    int result = sem_timedwait(&g_semaphore, &ts);
    if (result == 0) {
        return EPROTO_SIGNAL_DATA;
    } else {
        return EPROTO_SIGNAL_TIMEOUT;
    }
}

static void mock_signal_send(void) {
    if (g_semaphore_initialized) {
        sem_post(&g_semaphore);
    }
}'''
    
    content = re.sub(old_signal_wait, new_signal_wait, content)
    
    # Step 3: Add handshake success case to mock_status_callback
    status_callback_pattern = r'(void mock_status_callback\(eproto_status_t status, uint8_t\* data, uint16_t length\) \{[^}]*case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:[^}]*printf\("Status: Multiple CRC errors\\n"\);[^}]*break;)'
    if re.search(status_callback_pattern, content):
        new_case = '''        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("Status: Handshake success\\n");
            break;
'''
        content = re.sub(r'(case EPROTO_STATUS_MULTIPLE_CRC_ERRORS:[^}]*printf\("Status: Multiple CRC errors\\n"\);[^}]*break;)', 
                       r'\1\n' + new_case, content)
    
    # Step 4: Update user_functions to include signal_send and timeout_timestamp
    old_user_functions = '''eproto_user_functions_t user_functions = \\{.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = mock_signal_wait,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp\\};'''
    
    new_user_functions = '''eproto_user_functions_t user_functions = {.malloc = mock_malloc,
                                              .free = mock_free,
                                              .signal_wait = mock_signal_wait,
                                              .signal_send = mock_signal_send,
                                              .lock = mock_lock,
                                              .unlock = mock_unlock,
                                              .get_timestamp = mock_get_timestamp,
                                              .timeout_timestamp = 0};'''
    
    content = re.sub(old_user_functions, new_user_functions, content)
    
    # Step 5: Add protocol_thread function before print_help
    protocol_thread = '''void* protocol_thread(void* arg) {
    (void)arg;
    printf("Protocol thread started\\n");
    
    while (1) {
        eproto_tick(&g_eproto);
    }
    return NULL;
}

'''
    
    if 'void* protocol_thread' not in content:
        content = content.replace('void print_help(void) {', protocol_thread + 'void print_help(void) {')
    
    # Step 6: Create and start protocol thread
    # Find pthread_create lines for receive threads
    recv_thread_pattern = r'(pthread_t recv_thread_[a-z0-9_]+(?:, recv_thread_[a-z0-9_]+)*);'
    match = re.search(recv_thread_pattern, content)
    if match:
        old_thread_decl = match.group(1)
        new_thread_decl = old_thread_decl + ', proto_thread'
        content = content.replace(old_thread_decl, new_thread_decl)
        
        # Find pthread_create lines and add protocol thread creation
        create_lines_pattern = r'(pthread_create\(&recv_thread_[a-z0-9_]+, NULL, receive_thread_[a-z0-9_]+, NULL\);\s*)+'
        matches = list(re.finditer(create_lines_pattern, content))
        if matches:
            last_match = matches[-1]
            insert_pos = last_match.end()
            content = content[:insert_pos] + '\n    pthread_create(&proto_thread, NULL, protocol_thread, NULL);' + content[insert_pos:]
    
    # Step 7: Remove eproto_tick from main loop
    main_loop_pattern = r'        eproto_tick\(&g_eproto\);\s*usleep\(10000\);'
    content = re.sub(main_loop_pattern, '        usleep(10000);', content)
    
    # Write back the modified content
    if content != original_content:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f"Modified: {filepath}")
    else:
        print(f"No changes needed: {filepath}")

def main():
    files_to_modify = ['process_b.c', 'process_c.c', 'process_d.c', 'process_e.c']
    
    for filename in files_to_modify:
        filepath = os.path.join(os.path.dirname(__file__), filename)
        if os.path.exists(filepath):
            modify_file(filepath)
        else:
            print(f"File not found: {filepath}")

if __name__ == '__main__':
    main()
