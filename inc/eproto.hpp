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

#ifndef EPROTO_HPP
#define EPROTO_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <string>

extern "C" {
#include "eproto.h"
}

namespace eproto {

// Forward declarations
class Eproto;
class EprotoBus;

// Error code wrapper
enum class Error {
    Ok = EPROTO_OK,
    ErrorCRC = EPROTO_ERROR_CRC,
    ErrorTimeout = EPROTO_ERROR_TIMEOUT,
    ErrorBufferFull = EPROTO_ERROR_BUFFER_FULL,
    ErrorInvalidFrame = EPROTO_ERROR_INVALID_FRAME,
    ErrorMaxRetry = EPROTO_ERROR_MAX_RETRY,
    ErrorRouteNotFound = EPROTO_ERROR_ROUTE_NOT_FOUND,
    ErrorSleepFailed = EPROTO_ERROR_SLEEP_FAILED,
    ErrorWakeupFailed = EPROTO_ERROR_WAKEUP_FAILED,
    ErrorInvalidArgument = EPROTO_ERROR_INVALID_ARGUMENT
};

// Send status wrapper
enum class SendStatus {
    Success = EPROTO_SEND_SUCCESS,
    Timeout = EPROTO_SEND_TIMEOUT,
    Error = EPROTO_SEND_ERROR,
    Busy = EPROTO_SEND_BUSY
};

// Status wrapper
enum class Status {
    CRCError = EPROTO_STATUS_CRC_ERROR,
    MultipleCRCErrors = EPROTO_STATUS_MULTIPLE_CRC_ERRORS,
    HandshakeInProgress = EPROTO_STATUS_HANDSHAKE_IN_PROGRESS,
    HandshakeSuccess = EPROTO_STATUS_HANDSHAKE_SUCCESS
};

// Callback types using std::function
using PacketCallback = std::function<void(SendStatus status, uint16_t packetId,
                                          const uint8_t* data, uint16_t length,
                                          void* privateData)>;

using StatusCallback = std::function<void(EprotoBus& bus, Status status,
                                          const uint8_t* data, uint16_t length)>;

using ReceiveCallback = std::function<void(EprotoBus& bus, uint8_t srcAddr,
                                           uint16_t packetId, const uint8_t* data,
                                           uint16_t length)>;

using SendFunc = std::function<void(EprotoBus& bus, uint8_t* data, uint16_t length)>;

// User functions interface
class IUserFunctions {
public:
    virtual ~IUserFunctions() = default;
    virtual void* malloc(size_t size) = 0;
    virtual void free(void* ptr) = 0;
    virtual eproto_signal_result_t signalWait(uint32_t timestamp) = 0;
    virtual void signalSend() = 0;
    virtual void lock() = 0;
    virtual void unlock() = 0;
    virtual uint32_t getTimestamp() = 0;
};

// Default user functions using standard library
class DefaultUserFunctions : public IUserFunctions {
public:
    void* malloc(size_t size) override { return ::malloc(size); }
    void free(void* ptr) override { ::free(ptr); }
    eproto_signal_result_t signalWait(uint32_t /*timestamp*/) override { return EPROTO_SIGNAL_NO_PROGRESS; }
    void signalSend() override {}
    void lock() override {}
    void unlock() override {}
    uint32_t getTimestamp() override { return 0; }
};

// Bus class
class EprotoBus {
public:
    EprotoBus(uint8_t selfAddr, const std::string& name = "");
    ~EprotoBus() = default;

    // Setters
    void setSendFunc(SendFunc func) { m_sendFunc = std::move(func); }
    void setStatusCallback(StatusCallback callback) { m_statusCallback = std::move(callback); }
    void setReceiveCallback(ReceiveCallback callback) { m_receiveCallback = std::move(callback); }
    void setRxBuffer(std::vector<uint8_t>& buffer) { m_rxBuffer = buffer.data(); m_rxBufferSize = static_cast<uint16_t>(buffer.size()); }
    void setUserData(void* data) { m_userData = data; }

    // Getters
    uint8_t getSelfAddr() const { return m_selfAddr; }
    const std::string& getName() const { return m_name; }
    void* getUserData() const { return m_userData; }

    // Get the underlying C struct
    eproto_bus_t* getCBus() { return &m_cBus; }

private:
    friend class Eproto;

    uint8_t m_selfAddr;
    std::string m_name;
    SendFunc m_sendFunc;
    StatusCallback m_statusCallback;
    ReceiveCallback m_receiveCallback;
    uint8_t* m_rxBuffer = nullptr;
    uint16_t m_rxBufferSize = 0;
    void* m_userData = nullptr;
    eproto_bus_t m_cBus;

    // Static callback wrappers
    static void cSendFunc(eproto_bus_t* bus, uint8_t* data, uint16_t length);
    static void cStatusCallback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length);
    static void cReceiveCallback(eproto_bus_t* bus, uint8_t srcAddr, uint16_t packetId, uint8_t* data, uint16_t length);
};

// Main Eproto class
class Eproto {
public:
    Eproto();
    ~Eproto();

    // Non-copyable, movable
    Eproto(const Eproto&) = delete;
    Eproto& operator=(const Eproto&) = delete;
    Eproto(Eproto&&) noexcept = default;
    Eproto& operator=(Eproto&&) noexcept = default;

    // Initialize with custom user functions
    Error init(std::unique_ptr<IUserFunctions> userFunctions = nullptr);

    // Add a bus
    Error addBus(EprotoBus& bus);

    // Add destination device
    Error addDestinationDevice(uint8_t busAddr, uint8_t dstAddr);

    // Send data
    Error send(uint8_t dstAddr, const uint8_t* data, uint16_t length,
               PacketCallback callback = nullptr, void* privateData = nullptr,
               bool needReply = true);

    // Send with extended options
    Error sendEx(uint8_t dstAddr, const uint8_t* data, uint16_t length,
                 PacketCallback callback = nullptr, void* privateData = nullptr,
                 bool needReply = true, uint8_t maxRetryCount = EPROTO_DEFAULT_MAX_RETRY_COUNT,
                 uint32_t timeoutMs = EPROTO_DEFAULT_RETRY_TIMEOUT_MS);

    // Send user reply
    Error sendUserReply(uint8_t dstAddr, uint16_t packetId, const uint8_t* data, uint16_t length);

    // Send user reply with extended options
    Error sendUserReplyEx(uint8_t dstAddr, uint16_t packetId, const uint8_t* data,
                          uint16_t length, uint8_t maxRetryCount, uint32_t timeoutMs);

#if EPROTO_ENABLE_HANDSHAKE
    // Set handshake
    Error setHandshake(uint8_t busAddr, bool required);

    // Perform handshake
    Error handshake(uint8_t busAddr);
#endif

    // Receive data
    void receiveData(uint8_t busAddr, const uint8_t* data, size_t length);

    // Wait for signal
    bool waitForSignal();

    // Process
    uint32_t process();

    // Get status
    bool getStatus(uint8_t busAddr);

    // Get the underlying C struct
    eproto_t* getCProto() { return &m_cProto; }

private:
    eproto_t m_cProto;
    std::unique_ptr<IUserFunctions> m_userFunctions;
    std::vector<EprotoBus*> m_buses;

    // Static user function wrappers
    static void* cMalloc(void* userData, size_t size);
    static void cFree(void* userData, void* ptr);
    static eproto_signal_result_t cSignalWait(void* userData, uint32_t timestamp);
    static void cSignalSend(void* userData);
    static void cLock(void* userData);
    static void cUnlock(void* userData);
    static uint32_t cGetTimestamp(void* userData);

    // Static packet callback wrapper
    static void cPacketCallback(eproto_send_status_t status, uint16_t packetId,
                                uint8_t* data, uint16_t length, void* privateData);
};

// ============================================
// Implementation
// ============================================

// EprotoBus implementation
EprotoBus::EprotoBus(uint8_t selfAddr, const std::string& name)
    : m_selfAddr(selfAddr), m_name(name) {
    m_cBus.self_addr = selfAddr;
    m_cBus.name = m_name.empty() ? nullptr : m_name.c_str();
    m_cBus.send = cSendFunc;
    m_cBus.status_callback = cStatusCallback;
    m_cBus.receive_callback = cReceiveCallback;
    m_cBus.forward_callback = nullptr;
    m_cBus.rx_buffer = nullptr;
    m_cBus.rx_buffer_size = 0;
    m_cBus.user_data = this;
}

void EprotoBus::cSendFunc(eproto_bus_t* bus, uint8_t* data, uint16_t length) {
    auto* cppBus = static_cast<EprotoBus*>(bus->user_data);
    if (cppBus && cppBus->m_sendFunc) {
        cppBus->m_sendFunc(*cppBus, data, length);
    }
}

void EprotoBus::cStatusCallback(eproto_bus_t* bus, eproto_status_t status, uint8_t* data, uint16_t length) {
    auto* cppBus = static_cast<EprotoBus*>(bus->user_data);
    if (cppBus && cppBus->m_statusCallback) {
        cppBus->m_statusCallback(*cppBus, static_cast<Status>(status), data, length);
    }
}

void EprotoBus::cReceiveCallback(eproto_bus_t* bus, uint8_t srcAddr, uint16_t packetId, uint8_t* data, uint16_t length) {
    auto* cppBus = static_cast<EprotoBus*>(bus->user_data);
    if (cppBus && cppBus->m_receiveCallback) {
        cppBus->m_receiveCallback(*cppBus, srcAddr, packetId, data, length);
    }
}

// Eproto implementation
Eproto::Eproto() : m_userFunctions(std::make_unique<DefaultUserFunctions>()) {
    std::memset(&m_cProto, 0, sizeof(m_cProto));
}

Eproto::~Eproto() {
    eproto_destroy(&m_cProto);
}

Error Eproto::init(std::unique_ptr<IUserFunctions> userFunctions) {
    if (userFunctions) {
        m_userFunctions = std::move(userFunctions);
    }

    eproto_user_functions_t cUserFunctions;
    cUserFunctions.user_data = this;
    cUserFunctions.malloc = cMalloc;
    cUserFunctions.free = cFree;
    cUserFunctions.signal_wait = cSignalWait;
    cUserFunctions.signal_send = cSignalSend;
    cUserFunctions.lock = cLock;
    cUserFunctions.unlock = cUnlock;
    cUserFunctions.get_timestamp = cGetTimestamp;

    return static_cast<Error>(eproto_init(&m_cProto, &cUserFunctions));
}

Error Eproto::addBus(EprotoBus& bus) {
    // Update the C bus struct
    bus.m_cBus.self_addr = bus.m_selfAddr;
    bus.m_cBus.name = bus.m_name.empty() ? nullptr : bus.m_name.c_str();
    bus.m_cBus.rx_buffer = bus.m_rxBuffer;
    bus.m_cBus.rx_buffer_size = bus.m_rxBufferSize;
    bus.m_cBus.user_data = &bus;

    auto result = static_cast<Error>(eproto_add_bus(&m_cProto, &bus.m_cBus));
    if (result == Error::Ok) {
        m_buses.push_back(&bus);
    }
    return result;
}

Error Eproto::addDestinationDevice(uint8_t busAddr, uint8_t dstAddr) {
    return static_cast<Error>(eproto_add_destination_device(&m_cProto, busAddr, dstAddr));
}

Error Eproto::send(uint8_t dstAddr, const uint8_t* data, uint16_t length,
                   PacketCallback callback, void* privateData, bool needReply) {
    // Store callback in a way C can access it (simplified for now)
    // For a full implementation, you'd need a way to map privateData to the C++ callback
    eproto_packet_callback_t cCallback = nullptr;
    if (callback) {
        cCallback = cPacketCallback;
    }
    return static_cast<Error>(eproto_send(&m_cProto, dstAddr, const_cast<uint8_t*>(data),
                                          length, cCallback, privateData, needReply ? 1 : 0));
}

Error Eproto::sendEx(uint8_t dstAddr, const uint8_t* data, uint16_t length,
                     PacketCallback callback, void* privateData, bool needReply,
                     uint8_t maxRetryCount, uint32_t timeoutMs) {
    eproto_packet_callback_t cCallback = nullptr;
    if (callback) {
        cCallback = cPacketCallback;
    }
    return static_cast<Error>(eproto_send_ex(&m_cProto, dstAddr, const_cast<uint8_t*>(data),
                                             length, cCallback, privateData, needReply ? 1 : 0,
                                             maxRetryCount, timeoutMs));
}

Error Eproto::sendUserReply(uint8_t dstAddr, uint16_t packetId, const uint8_t* data, uint16_t length) {
    return static_cast<Error>(eproto_send_user_reply(&m_cProto, dstAddr, packetId,
                                                     const_cast<uint8_t*>(data), length));
}

Error Eproto::sendUserReplyEx(uint8_t dstAddr, uint16_t packetId, const uint8_t* data,
                              uint16_t length, uint8_t maxRetryCount, uint32_t timeoutMs) {
    return static_cast<Error>(eproto_send_user_reply_ex(&m_cProto, dstAddr, packetId,
                                                        const_cast<uint8_t*>(data), length,
                                                        maxRetryCount, timeoutMs));
}

#if EPROTO_ENABLE_HANDSHAKE
Error Eproto::setHandshake(uint8_t busAddr, bool required) {
    return static_cast<Error>(eproto_set_handshake(&m_cProto, busAddr, required ? 1 : 0));
}

Error Eproto::handshake(uint8_t busAddr) {
    return static_cast<Error>(eproto_handshake(&m_cProto, busAddr));
}
#endif

void Eproto::receiveData(uint8_t busAddr, const uint8_t* data, size_t length) {
    eproto_receive_data(&m_cProto, busAddr, data, length);
}

bool Eproto::waitForSignal() {
    return eproto_wait_for_signal(&m_cProto) != 0;
}

uint32_t Eproto::process() {
    return eproto_process(&m_cProto);
}

bool Eproto::getStatus(uint8_t busAddr) {
    return eproto_get_status(&m_cProto, busAddr) != 0;
}

// Static wrapper implementations
void* Eproto::cMalloc(void* userData, size_t size) {
    auto* proto = static_cast<Eproto*>(userData);
    return proto->m_userFunctions->malloc(size);
}

void Eproto::cFree(void* userData, void* ptr) {
    auto* proto = static_cast<Eproto*>(userData);
    proto->m_userFunctions->free(ptr);
}

eproto_signal_result_t Eproto::cSignalWait(void* userData, uint32_t timestamp) {
    auto* proto = static_cast<Eproto*>(userData);
    return proto->m_userFunctions->signalWait(timestamp);
}

void Eproto::cSignalSend(void* userData) {
    auto* proto = static_cast<Eproto*>(userData);
    proto->m_userFunctions->signalSend();
}

void Eproto::cLock(void* userData) {
    auto* proto = static_cast<Eproto*>(userData);
    proto->m_userFunctions->lock();
}

void Eproto::cUnlock(void* userData) {
    auto* proto = static_cast<Eproto*>(userData);
    proto->m_userFunctions->unlock();
}

uint32_t Eproto::cGetTimestamp(void* userData) {
    auto* proto = static_cast<Eproto*>(userData);
    return proto->m_userFunctions->getTimestamp();
}

void Eproto::cPacketCallback(eproto_send_status_t status, uint16_t packetId,
                             uint8_t* data, uint16_t length, void* privateData) {
    // Note: This is a simplified version. A full implementation would need
    // a way to map privateData back to the original C++ callback object.
    (void)status;
    (void)packetId;
    (void)data;
    (void)length;
    (void)privateData;
}

} // namespace eproto

#endif // EPROTO_HPP
