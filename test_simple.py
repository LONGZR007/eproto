
import time
import threading
from eproto import EprotoWrapper


class SimpleTest:
    def __init__(self):
        self.dev_a = EprotoWrapper()
        self.dev_b = EprotoWrapper()
        self.got_data = threading.Event()
        self.received_data = None

    def _send_a_to_b(self, data):
        print(f"[A→B] Send: {data!r}")
        self.dev_b.receive_data(2, data)

    def _on_b_recv(self, src, pkt_id, data):
        print(f"[B] Rcvd from {src} (id {pkt_id}): {data!r}")
        self.received_data = data
        self.got_data.set()

    def _process_loop(self, dev):
        while not self.got_data.is_set():
            dev.process()
            time.sleep(0.005)

    def run(self):
        # Init
        print("Init A")
        self.dev_a.init()
        print("Init B")
        self.dev_b.init()

        print("Add bus A")
        self.dev_a.add_bus(1, self._send_a_to_b, "bus_a")
        print("Add dest A→2")
        self.dev_a.add_destination_device(1, 2)

        print("Add bus B")
        self.dev_b.add_bus(2, lambda d: None, "bus_b", receive_callback=self._on_b_recv)
        print("Add dest B→1")
        self.dev_b.add_destination_device(2,1)

        # Start process threads
        ta = threading.Thread(target=self._process_loop, args=(self.dev_a,), daemon=True)
        tb = threading.Thread(target=self._process_loop, args=(self.dev_b,), daemon=True)
        ta.start()
        tb.start()

        time.sleep(0.1)

        print("\nSending data from A to B...")
        test_data = b"test123"
        self.dev_a.send(2, test_data, need_reply=0)

        if self.got_data.wait(timeout=2):
            if self.received_data == test_data:
                print("\n✅ Test passed!")
                return 0
            else:
                print(f"\n❌ Test failed: expected {test_data!r}, got {self.received_data!r}")
                return 1
        else:
            print("\n❌ Test timed out!")
            return 1


if __name__ == "__main__":
    t = SimpleTest()
    exit(t.run())
