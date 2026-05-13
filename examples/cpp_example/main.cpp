/*
 * MIT License
 *
 * Copyright (c) 2026 LONGZR007
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>

// Include the C++ wrapper
#include "eproto.hpp"

// Include our config
#include "eproto_config.h"

// Simulated hardware: two connected back-to-back
class SimulatedBus {
public:
    SimulatedBus(uint8_t addr1, uint8_t addr2) {
        m_buffer1.resize(256);
        m_buffer2.resize(256);
        m_addr1 = addr1;
        m_addr2 = addr2;
    }

    void setProto1Data(const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data1.insert(m_data1.end(), data, data + len);
    }

    void setProto2Data(const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data2.insert(m_data2.end(), data, data + len);
    }

    std::vector<uint8_t> getProto1Data() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint8_t> data = std::move(m_data2);
        m_data2.clear();
        return data;
    }

    std::vector<uint8_t> getProto2Data() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint8_t> data = std::move(m_data1);
        m_data1.clear();
        return data;
    }

    std::vector<uint8_t> m_buffer1, m_buffer2;
    uint8_t m_addr1, m_addr2;

private:
    std::vector<uint8_t> m_data1, m_data2;
    std::mutex m_mutex;
};

// Custom user functions with real timer
class CustomUserFunctions : public eproto::IUserFunctions {
public:
    void* malloc(size_t size) override { return std::malloc(size); }
    void free(void* ptr) override { std::free(ptr); }
    eproto_signal_result_t signalWait(uint32_t /*timestamp*/) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return EPROTO_SIGNAL_NO_PROGRESS;
    }
    void signalSend() override {}
    void lock() override { m_mutex.lock(); }
    void unlock() override { m_mutex.unlock(); }
    uint32_t getTimestamp() override {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }

private:
    std::mutex m_mutex;
};

// Global simulated bus
SimulatedBus* g_simBus = nullptr;

// Send function for proto 1 (addr 0x01)
void sendFunc1(eproto::EprotoBus& /*bus*/, uint8_t* data, uint16_t length) {
    std::cout << "[Proto1] Sending data: ";
    for (uint16_t i = 0; i < length; i++) {
        std::cout << std::hex << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    if (g_simBus) {
        g_simBus->setProto1Data(data, length);
    }
}

// Send function for proto 2 (addr 0x02)
void sendFunc2(eproto::EprotoBus& /*bus*/, uint8_t* data, uint16_t length) {
    std::cout << "[Proto2] Sending data: ";
    for (uint16_t i = 0; i < length; i++) {
        std::cout << std::hex << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    if (g_simBus) {
        g_simBus->setProto2Data(data, length);
    }
}

// Status callback
void statusCallback(eproto::EprotoBus& bus, eproto::Status status, const uint8_t* /*data*/, uint16_t /*length*/) {
    std::cout << "[" << static_cast<int>(bus.getSelfAddr()) << "] Status: ";
    switch (status) {
        case eproto::Status::CRCError:
            std::cout << "CRC Error";
            break;
        case eproto::Status::MultipleCRCErrors:
            std::cout << "Multiple CRC Errors";
            break;
        case eproto::Status::HandshakeInProgress:
            std::cout << "Handshake In Progress";
            break;
        case eproto::Status::HandshakeSuccess:
            std::cout << "Handshake Success!";
            break;
    }
    std::cout << std::endl;
}

// Receive callback for proto1
void receiveCallback1(eproto::EprotoBus& bus, uint8_t srcAddr, uint16_t packetId, const uint8_t* data, uint16_t length) {
    std::cout << "[Proto1] Received from " << static_cast<int>(srcAddr)
              << ", packet ID: " << packetId << ", data: ";
    for (uint16_t i = 0; i < length; i++) {
        std::cout << static_cast<char>(data[i]);
    }
    std::cout << std::endl;
}

// Receive callback for proto2
void receiveCallback2(eproto::EprotoBus& bus, uint8_t srcAddr, uint16_t packetId, const uint8_t* data, uint16_t length) {
    std::cout << "[Proto2] Received from " << static_cast<int>(srcAddr)
              << ", packet ID: " << packetId << ", data: ";
    for (uint16_t i = 0; i < length; i++) {
        std::cout << static_cast<char>(data[i]);
    }
    std::cout << std::endl;

    // Reply back
    const uint8_t reply[] = "Reply from 0x02";
    bus.getUserData(); // Just to avoid unused warning
}

int main() {
    std::cout << "eProto C++ Wrapper Example" << std::endl;
    std::cout << "========================" << std::endl << std::endl;

    // Create simulated bus
    SimulatedBus simBus(0x01, 0x02);
    g_simBus = &simBus;

    // Create protocol instances
    eproto::Eproto proto1;
    eproto::Eproto proto2;

    // Create buses
    eproto::EprotoBus bus1(0x01, "Bus 1");
    eproto::EprotoBus bus2(0x02, "Bus 2");

    // Configure buses
    bus1.setSendFunc(sendFunc1);
    bus1.setStatusCallback(statusCallback);
    bus1.setReceiveCallback(receiveCallback1);
    bus1.setRxBuffer(simBus.m_buffer1);

    bus2.setSendFunc(sendFunc2);
    bus2.setStatusCallback(statusCallback);
    bus2.setReceiveCallback(receiveCallback2);
    bus2.setRxBuffer(simBus.m_buffer2);

    // Initialize protocols
    std::cout << "Initializing protocols..." << std::endl;
    if (proto1.init(std::make_unique<CustomUserFunctions>()) != eproto::Error::Ok) {
        std::cerr << "Failed to init proto1" << std::endl;
        return 1;
    }
    if (proto2.init(std::make_unique<CustomUserFunctions>()) != eproto::Error::Ok) {
        std::cerr << "Failed to init proto2" << std::endl;
        return 1;
    }

    // Add buses
    std::cout << "Adding buses..." << std::endl;
    if (proto1.addBus(bus1) != eproto::Error::Ok) {
        std::cerr << "Failed to add bus1" << std::endl;
        return 1;
    }
    if (proto2.addBus(bus2) != eproto::Error::Ok) {
        std::cerr << "Failed to add bus2" << std::endl;
        return 1;
    }

    // Add destination devices
    std::cout << "Adding destination devices..." << std::endl;
    proto1.addDestinationDevice(0x01, 0x02);
    proto2.addDestinationDevice(0x02, 0x01);

#if EPROTO_ENABLE_HANDSHAKE
    // Disable handshake for simplicity in this loopback test
    proto1.setHandshake(0x01, false);
    proto2.setHandshake(0x02, false);
#endif

    // Send test message from proto1 to proto2
    const uint8_t testData[] = "Hello from 0x01!";
    std::cout << std::endl << "Sending test message from 0x01 to 0x02..." << std::endl;
    auto error = proto1.send(0x02, testData, sizeof(testData) - 1);
    if (error != eproto::Error::Ok) {
        std::cerr << "Failed to send message: " << static_cast<int>(error) << std::endl;
    }

    // Main processing loop
    std::cout << std::endl << "Processing for 2 seconds..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start).count() < 2) {
        // Process proto1
        {
            auto data = simBus.getProto1Data();
            if (!data.empty()) {
                proto2.receiveData(0x02, data.data(), data.size());
            }
            proto2.process();
        }

        // Process proto2
        {
            auto data = simBus.getProto2Data();
            if (!data.empty()) {
                proto1.receiveData(0x01, data.data(), data.size());
            }
            proto1.process();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << std::endl << "Example complete!" << std::endl;
    return 0;
}
