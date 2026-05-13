
import ctypes
import os

# Load lib
lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), "libeproto.so"))

# ==============================
# Define all C structs
# ==============================

# 1. eproto_list_head
class EprotoListHead(ctypes.Structure):
    pass
EprotoListHead._fields_ = [
    ("next", ctypes.POINTER(EprotoListHead)),
    ("prev", ctypes.POINTER(EprotoListHead)),
]

# 2. eproto_ring_buffer_t
class EprotoRingBuffer(ctypes.Structure):
    _fields_ = [
        ("buffer", ctypes.POINTER(ctypes.c_uint8)),
        ("size", ctypes.c_uint16),
        ("head", ctypes.c_uint16),
        ("tail", ctypes.c_uint16),
        ("count", ctypes.c_uint16),
    ]

# 3. eproto_frame_parser_config_t
class EprotoFrameParserConfig(ctypes.Structure):
    _fields_ = [
        ("frame_header", ctypes.c_uint8),
        ("max_frame_length", ctypes.c_uint16),
    ]

# 4. eproto_frame_parser_t
class EprotoFrameParser(ctypes.Structure):
    _fields_ = [
        ("config", EprotoFrameParserConfig),
        ("malloc_func", ctypes.c_void_p),
        ("free_func", ctypes.c_void_p),
    ]

# 5. eproto_bus_t (forward declare)
class EprotoBus(ctypes.Structure):
    pass

# Now define EprotoBus, use c_void_p for all callbacks
EprotoBus._fields_ = [
    ("self_addr", ctypes.c_uint8),
    ("send", ctypes.c_void_p),  # Use void pointer for callbacks
    ("rx_buffer", ctypes.POINTER(ctypes.c_uint8)),
    ("rx_buffer_size", ctypes.c_uint16),
    ("name", ctypes.c_char_p),
    ("user_data", ctypes.c_void_p),
    ("status_callback", ctypes.c_void_p),
    ("receive_callback", ctypes.c_void_p),
    ("forward_callback", ctypes.c_void_p),
]

# 6. eproto_device_queues_t
class EprotoDeviceQueues(ctypes.Structure):
    _fields_ = [
        ("send_queue", EprotoListHead),
        ("wait_queue", EprotoListHead),
        ("sending_queue", EprotoListHead),
    ]

# 7. eproto_bus_manager_t
MAX_DEST_DEVICES = 16  # From eproto_config.h
MAX_CONCURRENT_SENDS = 1
class EprotoBusManager(ctypes.Structure):
    _fields_ = [
        ("bus", EprotoBus),
        ("rx_buffer", EprotoRingBuffer),
        ("parser", EprotoFrameParser),
        ("next_packet_id", ctypes.c_uint16),
        ("last_ids", ctypes.c_uint16 * MAX_CONCURRENT_SENDS),
        ("last_id_index", ctypes.c_uint8),
        ("crc_error_count", ctypes.c_uint8),
        ("handshake_required", ctypes.c_uint8),  # Even if handshake disabled, keep field for struct size
        ("device_queues", EprotoDeviceQueues),
        ("destination_devices", ctypes.c_uint8 * MAX_DEST_DEVICES),
        ("destination_device_count", ctypes.c_uint8),
    ]

# 8. eproto_user_functions_t
# Use c_void_p for callbacks here too, then cast when we set them
class EprotoUserFunctions(ctypes.Structure):
    _fields_ = [
        ("malloc", ctypes.c_void_p),
        ("free", ctypes.c_void_p),
        ("signal_wait", ctypes.c_void_p),
        ("signal_send", ctypes.c_void_p),
        ("lock", ctypes.c_void_p),
        ("unlock", ctypes.c_void_p),
        ("get_timestamp", ctypes.c_void_p),
        ("timeout_timestamp", ctypes.c_uint32),
    ]

# 9. eproto_t
MAX_BUS_COUNT = 16  # From eproto_config.h
class Eproto(ctypes.Structure):
    _fields_ = [
        ("user_functions", EprotoUserFunctions),
        ("bus_managers", EprotoBusManager * MAX_BUS_COUNT),
    ]

# ==============================
# Define function types for when we need to create them
# ==============================
EprotoBusSendFunc = ctypes.CFUNCTYPE(None, ctypes.POINTER(EprotoBus), ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16)
C_MALLOC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_size_t)
C_FREE = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
C_SIGNAL_WAIT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_uint32)
C_SIGNAL_SEND = ctypes.CFUNCTYPE(None)
C_LOCK = ctypes.CFUNCTYPE(None)
C_UNLOCK = ctypes.CFUNCTYPE(None)
C_GET_TIMESTAMP = ctypes.CFUNCTYPE(ctypes.c_uint32)

# ==============================
# Define function signatures
# ==============================
lib.eproto_init.argtypes = [ctypes.POINTER(Eproto), ctypes.POINTER(EprotoUserFunctions)]
lib.eproto_init.restype = ctypes.c_int
lib.eproto_add_bus.argtypes = [ctypes.POINTER(Eproto), ctypes.POINTER(EprotoBus)]
lib.eproto_add_bus.restype = ctypes.c_int

# ==============================
# Test code
# ==============================

# Define simple user functions
def dummy_malloc(size):
    return ctypes.pythonapi.malloc(size)

def dummy_free(ptr):
    ctypes.pythonapi.free(ptr)

def dummy_signal_wait(ts):
    return 0

def dummy_signal_send():
    pass

def dummy_lock():
    pass

def dummy_unlock():
    pass

def dummy_get_timestamp():
    import time
    return int(time.time() * 1000)

# Wrap them
c_malloc = C_MALLOC(dummy_malloc)
c_free = C_FREE(dummy_free)
c_signal_wait = C_SIGNAL_WAIT(dummy_signal_wait)
c_signal_send = C_SIGNAL_SEND(dummy_signal_send)
c_lock = C_LOCK(dummy_lock)
c_unlock = C_UNLOCK(dummy_unlock)
c_get_timestamp = C_GET_TIMESTAMP(dummy_get_timestamp)

# Test step 1: Create user functions
user_funcs = EprotoUserFunctions(
    malloc=ctypes.cast(c_malloc, ctypes.c_void_p),
    free=ctypes.cast(c_free, ctypes.c_void_p),
    signal_wait=ctypes.cast(c_signal_wait, ctypes.c_void_p),
    signal_send=ctypes.cast(c_signal_send, ctypes.c_void_p),
    lock=ctypes.cast(c_lock, ctypes.c_void_p),
    unlock=ctypes.cast(c_unlock, ctypes.c_void_p),
    get_timestamp=ctypes.cast(c_get_timestamp, ctypes.c_void_p),
    timeout_timestamp=0
)
print("user_funcs created")

# Test step 2: Create eproto instance and init
eproto = Eproto()
print("Calling eproto_init...")
ret = lib.eproto_init(ctypes.byref(eproto), ctypes.byref(user_funcs))
print(f"eproto_init returned: {ret}")

# Test step 3: Prepare bus
def dummy_bus_send(bus, data, len):
    pass

c_bus_send = EprotoBusSendFunc(dummy_bus_send)

rx_buf = (ctypes.c_uint8 * 1024)()
bus = EprotoBus(
    self_addr=1,
    send=ctypes.cast(c_bus_send, ctypes.c_void_p),
    rx_buffer=ctypes.cast(rx_buf, ctypes.POINTER(ctypes.c_uint8)),
    rx_buffer_size=1024,
    name=b"test_bus",
    user_data=None,
    status_callback=None,
    receive_callback=None,
    forward_callback=None
)
print("Calling eproto_add_bus...")
ret = lib.eproto_add_bus(ctypes.byref(eproto), ctypes.byref(bus))
print(f"eproto_add_bus returned: {ret}")

print("Done!")
