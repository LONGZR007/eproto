
import ctypes
import os
import threading
import time
from typing import Callable, Optional, Any

# Load libc first
if os.name == "nt":
    libc = ctypes.CDLL("msvcrt.dll")
else:
    libc = ctypes.CDLL("libc.so.6")

# Set argtypes/restype for libc functions we use
libc.malloc.argtypes = [ctypes.c_size_t]
libc.malloc.restype = ctypes.c_void_p
libc.free.argtypes = [ctypes.c_void_p]
libc.free.restype = None

# Load shared library
lib_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "libeproto.so")
lib = ctypes.CDLL(lib_path)

# ==========================================
# C Type Definitions
# ==========================================

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

# Define EprotoBus with c_void_p for callbacks
EprotoBus._fields_ = [
    ("self_addr", ctypes.c_uint8),
    ("send", ctypes.c_void_p),
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
MAX_DEST_DEVICES = 16
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
        ("handshake_required", ctypes.c_uint8),
        ("device_queues", EprotoDeviceQueues),
        ("destination_devices", ctypes.c_uint8 * MAX_DEST_DEVICES),
        ("destination_device_count", ctypes.c_uint8),
    ]

# 8. eproto_user_functions_t
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
MAX_BUS_COUNT = 16
class Eproto(ctypes.Structure):
    _fields_ = [
        ("user_functions", EprotoUserFunctions),
        ("bus_managers", EprotoBusManager * MAX_BUS_COUNT),
    ]

# ==========================================
# Callback Function Types
# ==========================================
EprotoBusSendFunc = ctypes.CFUNCTYPE(None, ctypes.POINTER(EprotoBus), ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16)
EprotoStatusCallbackFunc = ctypes.CFUNCTYPE(None, ctypes.POINTER(EprotoBus), ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16)
ReceiveCallbackFunc = ctypes.CFUNCTYPE(None, ctypes.POINTER(EprotoBus), ctypes.c_uint8, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16)
EprotoPacketCallbackFunc = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_void_p)

# ==========================================
# Library Function Signatures
# ==========================================
lib.eproto_init.argtypes = [ctypes.POINTER(Eproto), ctypes.POINTER(EprotoUserFunctions)]
lib.eproto_init.restype = ctypes.c_int

lib.eproto_destroy.argtypes = [ctypes.POINTER(Eproto)]
lib.eproto_destroy.restype = None

lib.eproto_add_bus.argtypes = [ctypes.POINTER(Eproto), ctypes.POINTER(EprotoBus)]
lib.eproto_add_bus.restype = ctypes.c_int

lib.eproto_add_destination_device.argtypes = [ctypes.POINTER(Eproto), ctypes.c_uint8, ctypes.c_uint8]
lib.eproto_add_destination_device.restype = ctypes.c_int

lib.eproto_send.argtypes = [
    ctypes.POINTER(Eproto),
    ctypes.c_uint8,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint16,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_uint8,
]
lib.eproto_send.restype = ctypes.c_int

lib.eproto_send_ex.argtypes = [
    ctypes.POINTER(Eproto),
    ctypes.c_uint8,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint16,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_uint8,
    ctypes.c_uint32,
]
lib.eproto_send_ex.restype = ctypes.c_int

lib.eproto_receive_data.argtypes = [
    ctypes.POINTER(Eproto),
    ctypes.c_uint8,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
]
lib.eproto_receive_data.restype = None

lib.eproto_process.argtypes = [ctypes.POINTER(Eproto)]
lib.eproto_process.restype = ctypes.c_uint32

# ==========================================
# Global Helpers for User Functions
# ==========================================
_signal_event = threading.Event()
_lock = threading.Lock()

def _c_malloc(size):
    return libc.malloc(size)

def _c_free(ptr):
    libc.free(ptr)

def _c_signal_wait(ts):
    if _signal_event.wait(timeout=0.1):
        _signal_event.clear()
        return 0  # EPROTO_SIGNAL_DATA
    return 1  # EPROTO_SIGNAL_TIMEOUT

def _c_signal_send():
    _signal_event.set()

def _c_lock():
    _lock.acquire()

def _c_unlock():
    _lock.release()

def _c_get_timestamp():
    return int(time.time() * 1000)

# Wrap them as CFUNCTYPE to pass to C code
C_MALLOC = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_size_t)
C_FREE = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
_c_malloc_func = C_MALLOC(_c_malloc)
_c_free_func = C_FREE(_c_free)
_c_signal_wait_func = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_uint32)(_c_signal_wait)
_c_signal_send_func = ctypes.CFUNCTYPE(None)(_c_signal_send)
_c_lock_func = ctypes.CFUNCTYPE(None)(_c_lock)
_c_unlock_func = ctypes.CFUNCTYPE(None)(_c_unlock)
_c_get_timestamp_func = ctypes.CFUNCTYPE(ctypes.c_uint32)(_c_get_timestamp)


# ==========================================
# Main Wrapper Class
# ==========================================
class EprotoWrapper:
    def __init__(self):
        self._eproto = Eproto()
        self._user_funcs = EprotoUserFunctions(
            malloc=ctypes.cast(_c_malloc_func, ctypes.c_void_p),
            free=ctypes.cast(_c_free_func, ctypes.c_void_p),
            signal_wait=ctypes.cast(_c_signal_wait_func, ctypes.c_void_p),
            signal_send=ctypes.cast(_c_signal_send_func, ctypes.c_void_p),
            lock=ctypes.cast(_c_lock_func, ctypes.c_void_p),
            unlock=ctypes.cast(_c_unlock_func, ctypes.c_void_p),
            get_timestamp=ctypes.cast(_c_get_timestamp_func, ctypes.c_void_p),
            timeout_timestamp=0,
        )
        self._rx_buffers = {}
        self._bus_send_funcs = {}
        self._bus_status_callbacks = {}
        self._bus_recv_callbacks = {}
        self._packet_callbacks = {}  # To keep references alive

    def init(self):
        return lib.eproto_init(ctypes.byref(self._eproto), ctypes.byref(self._user_funcs))

    def destroy(self):
        lib.eproto_destroy(ctypes.byref(self._eproto))

    def add_bus(
        self,
        self_addr: int,
        send_func: Callable[[bytes], None],
        name: str,
        status_callback: Optional[Callable[[int, bytes], None]] = None,
        receive_callback: Optional[Callable[[int, int, bytes], None]] = None,
    ):
        # Create receive buffer
        rx_buffer_size = 1024
        rx_buffer = (ctypes.c_uint8 * rx_buffer_size)()
        self._rx_buffers[self_addr] = rx_buffer

        # Create send callback wrapper
        def _bus_send_wrapper(bus_ptr, data_ptr, length):
            data = bytes(ctypes.string_at(data_ptr, length))
            send_func(data)

        c_send_func = EprotoBusSendFunc(_bus_send_wrapper)
        self._bus_send_funcs[self_addr] = c_send_func

        # Status callback wrapper
        c_status_func = None
        if status_callback:
            def _status_wrapper(bus_ptr, status, data_ptr, length):
                data = bytes(ctypes.string_at(data_ptr, length)) if data_ptr else b""
                status_callback(status, data)
            c_status_func = EprotoStatusCallbackFunc(_status_wrapper)
            self._bus_status_callbacks[self_addr] = c_status_func

        # Receive callback wrapper
        c_recv_func = None
        if receive_callback:
            def _recv_wrapper(bus_ptr, src_addr, packet_id, data_ptr, length):
                data = bytes(ctypes.string_at(data_ptr, length)) if data_ptr else b""
                receive_callback(src_addr, packet_id, data)
            c_recv_func = ReceiveCallbackFunc(_recv_wrapper)
            self._bus_recv_callbacks[self_addr] = c_recv_func

        bus = EprotoBus(
            self_addr=self_addr,
            send=ctypes.cast(c_send_func, ctypes.c_void_p),
            rx_buffer=ctypes.cast(rx_buffer, ctypes.POINTER(ctypes.c_uint8)),
            rx_buffer_size=rx_buffer_size,
            name=name.encode("utf-8"),
            user_data=None,
            status_callback=ctypes.cast(c_status_func, ctypes.c_void_p) if c_status_func else None,
            receive_callback=ctypes.cast(c_recv_func, ctypes.c_void_p) if c_recv_func else None,
            forward_callback=None,
        )
        return lib.eproto_add_bus(ctypes.byref(self._eproto), ctypes.byref(bus))

    def add_destination_device(self, self_addr: int, dest_addr: int):
        return lib.eproto_add_destination_device(ctypes.byref(self._eproto), self_addr, dest_addr)

    def send(
        self,
        dest_addr: int,
        data: bytes,
        callback: Optional[Callable[[int, int, bytes], None]] = None,
        need_reply: int = 1,
    ):
        data_arr = (ctypes.c_uint8 * len(data))(*data)
        c_callback = None
        if callback:
            def _packet_wrapper(status, packet_id, data_ptr, length, user_data):
                data = bytes(ctypes.string_at(data_ptr, length)) if data_ptr else b""
                callback(status, packet_id, data)
            c_callback = EprotoPacketCallbackFunc(_packet_wrapper)
            self._packet_callbacks[(dest_addr, id(callback))] = c_callback
        return lib.eproto_send(
            ctypes.byref(self._eproto),
            dest_addr,
            data_arr,
            len(data),
            ctypes.cast(c_callback, ctypes.c_void_p) if c_callback else None,
            None,
            need_reply,
        )

    def receive_data(self, bus_addr: int, data: bytes):
        data_arr = (ctypes.c_uint8 * len(data))(*data)
        lib.eproto_receive_data(ctypes.byref(self._eproto), bus_addr, data_arr, len(data))

    def process(self):
        return lib.eproto_process(ctypes.byref(self._eproto))
