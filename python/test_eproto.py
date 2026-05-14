
import time
import threading
from eproto import EprotoWrapper


class TestEprotoCommunication:
    def __init__(self):
        self.device_a = EprotoWrapper()
        self.device_b = EprotoWrapper()
        self.received_from_a = None
        self.received_from_b = None
        self.test_done = threading.Event()
        self.process_threads = []

    def setup(self):
        # Initialize both devices
        print("Initializing Device A...")
        ret = self.device_a.init()
        print(f"Device A init result: {ret}")

        print("\nInitializing Device B...")
        ret = self.device_b.init()
        print(f"Device B init result: {ret}")

        # Add buses to both devices
        print("\nAdding bus to Device A (addr 1)...")
        ret = self.device_a.add_bus(
            self_addr=1,
            send_func=self._send_from_a_to_b,
            name="bus_a",
            receive_callback=self._on_receive_from_b,
        )
        print(f"Device A add_bus result: {ret}")

        print("\nAdding destination device (addr 2) to Device A...")
        ret = self.device_a.add_destination_device(self_addr=1, dest_addr=2)
        print(f"Device A add_destination_device result: {ret}")

        print("\nAdding bus to Device B (addr 2)...")
        ret = self.device_b.add_bus(
            self_addr=2,
            send_func=self._send_from_b_to_a,
            name="bus_b",
            receive_callback=self._on_receive_from_a,
        )
        print(f"Device B add_bus result: {ret}")

        print("\nAdding destination device (addr 1) to Device B...")
        ret = self.device_b.add_destination_device(self_addr=2, dest_addr=1)
        print(f"Device B add_destination_device result: {ret}")

    def _send_from_a_to_b(self, data: bytes):
        print(f"\n[A→B] Sending data (len={len(data)}): {data.hex()}")
        self.device_b.receive_data(2, data)

    def _send_from_b_to_a(self, data: bytes):
        print(f"\n[B→A] Sending data (len={len(data)}): {data.hex()}")
        self.device_a.receive_data(1, data)

    def _on_receive_from_a(self, src_addr: int, packet_id: int, data: bytes):
        print(f"\n[B] Received from {src_addr} (packet {packet_id}): {data!r}")
        self.received_from_a = data
        # Send reply back
        reply = b"Reply: " + data
        print(f"[B] Sending reply: {reply!r}")
        self.device_b.send(1, reply, need_reply=0)

    def _on_receive_from_b(self, src_addr: int, packet_id: int, data: bytes):
        print(f"\n[A] Received from {src_addr} (packet {packet_id}): {data!r}")
        self.received_from_b = data
        self.test_done.set()

    def _run_process_loop(self, device: EprotoWrapper):
        while not self.test_done.is_set():
            device.process()
            time.sleep(0.01)

    def run_test(self):
        print("\n=== Starting test ===")
        # Start process threads for both devices
        self.process_threads.append(threading.Thread(target=self._run_process_loop, args=(self.device_a,), daemon=True))
        self.process_threads.append(threading.Thread(target=self._run_process_loop, args=(self.device_b,), daemon=True))
        for t in self.process_threads:
            t.start()

        # Wait a little for things to settle
        time.sleep(0.2)

        # Send test data from A to B
        test_data = b"Hello eProto!"
        print(f"\n[A] Sending test data: {test_data!r}")
        self.device_a.send(2, test_data, callback=self._on_send_complete, need_reply=1)

        # Wait for test completion (timeout after 5 seconds)
        if self.test_done.wait(timeout=5.0):
            print("\n=== Test completed successfully! ===")
        else:
            print("\n=== Test timed out! ===")

        # Verify results
        print("\n=== Verifying results ===")
        all_passed = True
        if self.received_from_a == test_data:
            print("✓ Device B received correct data from A")
        else:
            print(f"✗ Device B received wrong data: expected {test_data!r}, got {self.received_from_a!r}")
            all_passed = False
        expected_reply = b"Reply: " + test_data
        if self.received_from_b == expected_reply:
            print("✓ Device A received correct reply from B")
        else:
            print(f"✗ Device A received wrong reply: expected {expected_reply!r}, got {self.received_from_b!r}")
            all_passed = False

        # Cleanup
        print("\n=== Cleaning up ===")
        self.device_a.destroy()
        self.device_b.destroy()

        return all_passed

    def _on_send_complete(self, status: int, packet_id: int, data: bytes):
        print(f"\n[A] Send completed: status={status}, packet_id={packet_id}, data={data!r}")


if __name__ == "__main__":
    test = TestEprotoCommunication()
    test.setup()
    passed = test.run_test()
    exit(0 if passed else 1)
