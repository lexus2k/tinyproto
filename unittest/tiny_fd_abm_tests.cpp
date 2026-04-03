/*
    Copyright 2019-2025 (C) Alexey Dynda

    This file is part of Tiny Protocol Library.

    GNU General Public License Usage

    Protocol Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Protocol Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Protocol Library.  If not, see <http://www.gnu.org/licenses/>.

    Commercial License Usage

    Licensees holding valid commercial Tiny Protocol licenses may use this file in
    accordance with the commercial license agreement provided in accordance with
    the terms contained in a written agreement between you and Alexey Dynda.
    For further information contact via email on github account.
*/

#include <functional>
#include <CppUTest/TestHarness.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <array>

#include "proto/fd/tiny_fd.h"

TEST_GROUP(TINY_FD_ABM)
{
    void setup()
    {
        connected = false;
        tiny_fd_init_t init{};
        init.pdata = this;
        init.on_connect_event_cb = __onConnect;
        init.on_read_cb = onRead;
        init.on_send_cb = onSend;
        init.log_frame_cb = logFrame;
        init.buffer = inBuffer.data();
        init.buffer_size = inBuffer.size();
        init.window_frames = 7;
        init.send_timeout = 1000;
        init.retry_timeout = 100;
        init.retries = 2;
        init.mode = TINY_FD_MODE_ABM;
        init.peers_count = 1; // For ABM mode, only one peer is needed
        init.crc_type = HDLC_CRC_OFF;
        auto result = tiny_fd_init(&handle, &init);
        CHECK_EQUAL(TINY_SUCCESS, result);
    }

    void teardown()
    {
        tiny_fd_close(handle);
        logFrameFunc = nullptr;
    }

    void onConnect(uint8_t, bool status) { connected = status; }
    void onRead(uint8_t, uint8_t *, int) { }
    void onSend(uint8_t, const uint8_t *, int) { }

    static void __onConnect(void *udata, uint8_t address, bool connected)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_ABM *>(udata);
        self->onConnect(address, connected);
    }

    static void onRead(void *udata, uint8_t address, uint8_t *buf, int len)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_ABM *>(udata);
        self->onRead(address, buf, len);
    }

    static void onSend(void *udata, uint8_t address, const uint8_t *buf, int len)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_ABM *>(udata);
        self->onSend(address, buf, len);
    }

    static void logFrame(void *udata, tiny_fd_handle_t handle, tiny_fd_frame_direction_t direction,
                        tiny_fd_frame_type_t frame_type, tiny_fd_frame_subtype_t frame_subtype,
                        uint8_t ns, uint8_t nr, const uint8_t *data, int len)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_ABM *>(udata);
        if (!self->logFrameFunc) {
            return; // No logging function set
        }
        self->logFrameFunc(handle, direction, frame_type, frame_subtype, ns, nr, data, len);
    }

    tiny_fd_handle_t handle = nullptr;
    bool connected = false;
    std::array<uint8_t, 1024> inBuffer{};
    std::array<uint8_t, 1024> outBuffer{};
    std::function<void(tiny_fd_handle_t, tiny_fd_frame_direction_t,
                       tiny_fd_frame_type_t, tiny_fd_frame_subtype_t, uint8_t, uint8_t,
                       const uint8_t *, int)> logFrameFunc = nullptr;

    void establishConnection()
    {
        // For command requests CR bit must be set, that is why address is 0x03
        auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x2F\x7E", 4); // SABM frame
        CHECK_EQUAL(TINY_SUCCESS, read_result);
        auto len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
        CHECK(connected); // Connection should be established
        CHECK_EQUAL(4, len);
    }
    
    void reinitializeWithMtu(int mtu)
    {
        tiny_fd_close(handle); // Close the previous handle
        tiny_fd_init_t init{};
        init.pdata = this;
        init.on_connect_event_cb = __onConnect;
        init.on_read_cb = onRead;
        init.on_send_cb = onSend;
        init.log_frame_cb = logFrame;
        init.buffer = inBuffer.data();
        init.buffer_size = inBuffer.size();
        init.window_frames = 7;
        init.send_timeout = 1000;
        init.retry_timeout = 100;
        init.retries = 2;
        init.mode = TINY_FD_MODE_ABM;
        init.peers_count = 1; // For ABM mode, only one peer is needed
        init.crc_type = HDLC_CRC_OFF;
        init.mtu = mtu; // Set MTU
        auto result = tiny_fd_init(&handle, &init);
        CHECK_EQUAL(TINY_SUCCESS, result);
    }

};


TEST(TINY_FD_ABM, ABM_ConnectDisconnectResponse)
{
    // For command requests CR bit must be set, that is why address is 0x03
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x2F\x7E", 4); // SABM frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check UA frame
    // UA frame should be: 0x7E 0x03 0x00 0x7E
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    CHECK(connected); // Connection should be established
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x43\x7E", 4); // DISC frame
    CHECK_EQUAL(TINY_SUCCESS, read_result); // Should read 7 bytes
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check UA frame
    // UA frame should be: 0x7E 0x03 0x00 0x7E
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit must be cleared
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    CHECK(!connected); // Connection should be disconnected 
}

TEST(TINY_FD_ABM, ABM_DisconnectResponseWhenNotConnected)
{
    connected = true; // this flag should not be changed if not connected
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x43\x7E", 4); // DISC frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Per HDLC spec: DISC when disconnected gets DM response (not UA)
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // address field - CR bit must be cleared
    CHECK_EQUAL(0x1F, outBuffer[2]); // DM packet with P/F bit
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    CHECK(connected); // Connection should not be changed
}

TEST(TINY_FD_ABM, ABM_RecieveTwoConsequentIFrames)
{
    establishConnection();
    // Now we can send I-frames
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame with data
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x02\x22\x7E", 5); // I-frame with data
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 0);
    len += tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 100);
    CHECK_EQUAL(8, len);
    // Check RR frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit must be cleared
    CHECK_EQUAL(0x21, outBuffer[2]); // RR packet with N(R) = 1
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    // Check RR frame
    CHECK_EQUAL(0x7E, outBuffer[4]); // Flag
    CHECK_EQUAL(0x01, outBuffer[5]); // Address field - CR bit must be cleared
    CHECK_EQUAL(0x41, outBuffer[6]); // RR packet with N(R) = 2
    CHECK_EQUAL(0x7E, outBuffer[7]); // Flag
    // Now we can send I-frames again
}

TEST(TINY_FD_ABM, ABM_RecieveOutOfOrderIFrames)
{
    establishConnection();
    // Now we can send I-frames
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame in order
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x04\x22\x7E", 5); // I-frame out of order
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    // Sometimes TX operation can be split into multiple calls,
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 0);
    len += tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 100);
    CHECK_EQUAL(8, len);
    // Check RR frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit must be cleared
    CHECK_EQUAL(0x21, outBuffer[2]); // RR packet with N(R) = 1
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    // Check RR frame
    CHECK_EQUAL(0x7E, outBuffer[4]); // Flag
    CHECK_EQUAL(0x03, outBuffer[5]); // Address field - CR bit set (REJ is a command)
    CHECK_EQUAL(0x29, outBuffer[6]); // REJ packet with N(R) = 1
    CHECK_EQUAL(0x7E, outBuffer[7]); // Flag
}

TEST(TINY_FD_ABM, ABM_SendDMOnIFrameIfDisconnected)
{
    // Per HDLC ABM spec, if we are disconnected and receive I-frame, send DM (Disconnect Mode)
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame in order
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    auto len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // DM control = (0x0C | 0x03) | P_BIT = 0x0F | 0x10 = 0x1F
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit cleared (response)
    CHECK_EQUAL(0x1F, outBuffer[2]); // DM packet with P/F bit
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
}

TEST(TINY_FD_ABM, ABM_RunRxAPIVerification)
{
    // For command requests CR bit must be set, that is why address is 0x03
    auto read_result = tiny_fd_run_rx(handle, [](void *user_data, void *buf, int len) -> int {
        // Simulate reading data from a source
        memcpy(buf, "\x7E\x03\x2F\x7E", 4); // SABM frame
        return 4; // Return number of bytes read
    });
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check UA frame
    // UA frame should be: 0x7E 0x03 0x00 0x7E
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
}

TEST(TINY_FD_ABM, ABM_RunTxAPIVerification)
{
    // For command requests CR bit must be set, that is why address is 0x03
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x2F\x7E", 4); // SABM frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_run_tx(handle, [](void *user_data, const void *buf, int len) -> int {
        CHECK_EQUAL(4, len); // Expecting to write 4 bytes
        CHECK_EQUAL(0x7E, ((const uint8_t *)buf)[0]); // Flag
        CHECK_EQUAL(0x01, ((const uint8_t *)buf)[1]); // Address field
        CHECK_EQUAL(0x73, ((const uint8_t *)buf)[2]); // UA packet
        CHECK_EQUAL(0x7E, ((const uint8_t *)buf)[3]); // Flag
        return len; // Return number of bytes written
    });
    CHECK_EQUAL(TINY_SUCCESS, len);
}

TEST(TINY_FD_ABM, ABM_CheckMtuAPI)
{
    // Check MTU API
    int mtu = tiny_fd_get_mtu(handle);
    CHECK(mtu > 0); // MTU should be greater than 0
    CHECK_EQUAL(32, mtu); // MTU based on 1024-byte buffer and protocol overhead
}

TEST(TINY_FD_ABM, ABM_CheckLoggerFunction)
{
    int counter = 0;
    // Check logger function
    auto log_frame_func = [&counter](tiny_fd_handle_t handle,
                                             tiny_fd_frame_direction_t direction,
                                             tiny_fd_frame_type_t frame_type,
                                             tiny_fd_frame_subtype_t frame_subtype,
                                             uint8_t ns,
                                             uint8_t nr,
                                             const uint8_t *data,
                                             int len) {
        switch (counter) {
            case 0: // SABM frame
                CHECK_EQUAL(TINY_FD_FRAME_TYPE_U, frame_type);
                CHECK_EQUAL(TINY_FD_FRAME_SUBTYPE_SABM, frame_subtype);
                CHECK_EQUAL(0x00, ns);
                CHECK_EQUAL(0x00, nr);
                CHECK_EQUAL(2, len);
                CHECK_EQUAL(0x03, data[0]); // Address field
                CHECK_EQUAL(0x2F, data[1]); // SABM packet
                break;
            case 1: // UA frame
                CHECK_EQUAL(TINY_FD_FRAME_TYPE_U, frame_type);
                CHECK_EQUAL(TINY_FD_FRAME_SUBTYPE_UA, frame_subtype);
                CHECK_EQUAL(0x00, ns);
                CHECK_EQUAL(0x00, nr);
                CHECK_EQUAL(2, len);
                CHECK_EQUAL(0x01, data[0]); // Address field
                CHECK_EQUAL(0x73, data[1]); // UA packet
                break;
            case 2: // I-frame
                CHECK_EQUAL(TINY_FD_FRAME_TYPE_I, frame_type);
                CHECK_EQUAL(TINY_FD_FRAME_SUBTYPE_RR, frame_subtype); // Should be RR frame
                CHECK_EQUAL(0x00, ns); // N(S) = 0
                CHECK_EQUAL(0x00, nr); // N(R) = 1
                CHECK_EQUAL(3, len);
                CHECK_EQUAL(0x03, data[0]); // Address field
                CHECK_EQUAL(0x00, data[1]); // Data byte
                CHECK_EQUAL(0x11, data[2]); // Data byte
                break;
            default:
                FAIL("Unexpected frame logged");
                break;
        }
        counter++;
    };
    logFrameFunc = log_frame_func; // Set the logging function
    establishConnection(); // This will trigger the logging function for SABM and UA frames
    CHECK_EQUAL(2, counter); // We should have logged 2 frames: SABM and UA
    // Now we can send I-frames
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame in order
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK_EQUAL(3, counter); // We should have logged 3 frames now
}

TEST(TINY_FD_ABM, ABM_CheckMtuAndSendSplit)
{
    // Check MTU reinitialization
    reinitializeWithMtu(2); // Set new MTU
    int mtu = tiny_fd_get_mtu(handle);
    CHECK_EQUAL(2, mtu); // MTU should be equal to 2
    establishConnection(); // Establish connection with new MTU
    // Now we can send I-frames
    // Check that 5-byte frame is split into 3 frames of 2 bytes each
    int result = tiny_fd_send_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\x01\x02\x03\x04\x05", 5, 1000);
    CHECK_EQUAL(5, result); // We should have sent 5 bytes
    // Sometimes TX operation can be split into multiple calls,
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 0);
    len += tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 0);
    len += tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 100);
    CHECK_EQUAL(17, len); // We should have 3 I-frames of 2 bytes, 2 bytes and 1 byte
    // Check first I-frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field
    CHECK_EQUAL(0x00, outBuffer[2]); // Control byte: I-frame N(S)=0,N(R)=0
    CHECK_EQUAL(0x01, outBuffer[3]); // Data byte
    CHECK_EQUAL(0x02, outBuffer[4]); // Data byte
    CHECK_EQUAL(0x7E, outBuffer[5]); // Flag
    // Check second I-frame
    CHECK_EQUAL(0x7E, outBuffer[6]); // Flag
    CHECK_EQUAL(0x01, outBuffer[7]); // Address field
    CHECK_EQUAL(0x02, outBuffer[8]); // Control byte: I-frame N(S)=1,N(R)=0
    CHECK_EQUAL(0x03, outBuffer[9]); // Data byte
    CHECK_EQUAL(0x04, outBuffer[10]); // Data byte
    CHECK_EQUAL(0x7E, outBuffer[11]); // Flag
    // Check third I-frame
    CHECK_EQUAL(0x7E, outBuffer[12]); // Flag
    CHECK_EQUAL(0x01, outBuffer[13]); // Address field
    CHECK_EQUAL(0x04, outBuffer[14]); // Control byte: I-frame N(S)=2,N(R)=0
    CHECK_EQUAL(0x05, outBuffer[15]); // Data byte
    CHECK_EQUAL(0x7E, outBuffer[16]); // Flag
    // Check that I-frame larger than mtu size cannot be sent
    result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\x01\x02\x03", 3, 1000);
    CHECK_EQUAL(TINY_ERR_DATA_TOO_LARGE, result); // Should return error for frame larger than MTU
}

TEST(TINY_FD_ABM, ABM_CheckReceiveReadyWithCommandBitSet)
{
    // On Receive Ready with command bit set, and if there is no data to send,
    // we should send RR frame with command bit cleared
    establishConnection(); // Establish connection first
    // Emulate receiving a Receive Ready frame with poll bit set
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x11\x7E", 4); // RR frame with poll bit set
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len); // We should have sent RR frame
    // Check RR frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit must be cleared
    CHECK_EQUAL(0x11, outBuffer[2]); // RR packet with N(R) = 0
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
}

TEST(TINY_FD_ABM, ABM_CheckReceiveReadyWithCommandBitCleared)
{
    // On Receive Ready with command bit set, and if there is no data to send,
    // we should send RR frame with command bit cleared
    establishConnection(); // Establish connection first
    // Emulate receiving a Receive Ready frame with poll bit set
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x01\x11\x7E", 4); // RR frame with poll bit set
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(0, len); // We should have sent RR frame
}

TEST(TINY_FD_ABM, ABM_SendDMOnSFrameIfDisconnected)
{
    // Per HDLC ABM spec, if we are disconnected and receive S-frame, send DM
    // RR frame with N(R)=0: control = 0x01
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x01\x7E", 4); // RR S-frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    auto len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // DM control = (0x0C | 0x03) | P_BIT = 0x0F | 0x10 = 0x1F
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit cleared (response)
    CHECK_EQUAL(0x1F, outBuffer[2]); // DM packet with P/F bit
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
}

TEST(TINY_FD_ABM, ABM_FRMRReceiveTriggersReconnect)
{
    // FRMR should trigger disconnection and SABM reconnection attempt
    establishConnection();
    CHECK(connected);
    // Send FRMR frame: control = HDLC_U_FRAME_TYPE_FRMR | HDLC_U_FRAME_BITS = 0x84 | 0x03 = 0x87
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x01\x87\x00\x00\x7E", 6); // FRMR with 2 info bytes
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK(!connected); // Should have disconnected
    // Should get SABM reconnection attempt
    auto len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x03, outBuffer[1]); // Address field - CR bit set (command)
    CHECK_EQUAL(0x3F, outBuffer[2]); // SABM packet (0x2C | 0x13 with P bit = 0x2F | 0x10 = 0x3F)
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
}

TEST(TINY_FD_ABM, ABM_RSETResetsSequenceNumbers)
{
    establishConnection();
    // Send some I-frames to advance sequence numbers
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame N(S)=0
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    // Drain RR response
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    // Now send RSET: control = HDLC_U_FRAME_TYPE_RSET | HDLC_U_FRAME_BITS = 0x8C | 0x03 = 0x8F
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x8F\x7E", 4); // RSET frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    // Should respond with UA
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field - CR bit cleared (response)
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    // After RSET, send I-frame N(S)=0 again — should succeed since V(R) was reset to 0
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x22\x7E", 5); // I-frame N(S)=0
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    // Should get RR with N(R)=1 confirming the frame was accepted
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field
    CHECK_EQUAL(0x21, outBuffer[2]); // RR with N(R)=1
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
}

TEST(TINY_FD_ABM, ABM_RNRPausesTransmission)
{
    establishConnection();
    // Send I-frame first to advance sequence
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    // Drain RR
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    // Send RNR S-frame: control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RNR | (N(R)<<5)
    // = 0x01 | 0x04 | (1<<5) = 0x25
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x25\x7E", 4); // RNR with N(R)=1
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    // RNR should be accepted without error (confirms N(R)=1)
    // No response frame should be generated (RNR doesn't have CR bit set as command here)
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(0, len);
}

TEST(TINY_FD_ABM, ABM_DMReceiveDisconnects)
{
    establishConnection();
    CHECK(connected);
    // Receive DM from peer: control = HDLC_U_FRAME_TYPE_DM | HDLC_U_FRAME_BITS = 0x0C | 0x03 = 0x0F
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x01\x0F\x7E", 4); // DM frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK(!connected); // Should have disconnected
}

// ==========================================================================
// P/F Bit Enforcement Tests
// ==========================================================================

TEST(TINY_FD_ABM, ABM_PFBit_UFramesAlwaysHavePBit)
{
    // U-frames (SABM, UA, DM, DISC) should always have P/F bit set in ABM
    // Send SABM, check UA response has P/F bit
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x2F\x7E", 4); // SABM
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x73, outBuffer[2]); // UA = 0x63 | P_BIT(0x10) = 0x73

    // Send DISC, check UA response has P/F bit
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x43\x7E", 4); // DISC
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x73, outBuffer[2]); // UA with P/F bit

    // Send I-frame while disconnected, check DM has P/F bit
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5);
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x1F, outBuffer[2]); // DM = 0x0F | P_BIT(0x10) = 0x1F
}

TEST(TINY_FD_ABM, ABM_PFBit_IFramesNoPBit)
{
    // I-frames should NOT have P bit set in ABM mode
    establishConnection();
    int result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xAA", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    // I-frame control: N(R)=0, N(S)=0, no P bit = 0x00
    CHECK_EQUAL(0x00, outBuffer[2] & 0x10); // P bit should NOT be set
}

TEST(TINY_FD_ABM, ABM_PFBit_RRResponseNoPBit)
{
    // RR response after receiving I-frame should NOT have P bit in ABM
    establishConnection();
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x00\x11\x7E", 5); // I-frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    // RR control: N(R)=1, RR, no P bit = 0x21
    CHECK_EQUAL(0x00, outBuffer[2] & 0x10); // P bit should NOT be set
    CHECK_EQUAL(0x21, outBuffer[2]); // RR with N(R)=1, no P
}

TEST(TINY_FD_ABM, ABM_PFBit_PropagatesPToFInResponse)
{
    // When RR command has P=1, response should have F=1
    establishConnection();
    // RR with CR bit and P bit: address=0x03, control=0x11 (P=1, RR, N(R)=0)
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x11\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x11, outBuffer[2]); // RR response with F=1 (propagated from P=1)

    // Now send RR command WITHOUT P bit: address=0x03, control=0x01 (P=0, RR, N(R)=0)
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x01\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x01, outBuffer[2]); // RR response with F=0 (no P in command)
}

// ==========================================================================
// UI (Unnumbered Information) Frame Tests
// ==========================================================================

TEST(TINY_FD_ABM, ABM_UIFrameSend)
{
    // UI frames can be sent even when disconnected (connectionless)
    int result = tiny_fd_send_ui_packet(handle, (const void *)"\xDE\xAD", 2);
    CHECK_EQUAL(TINY_SUCCESS, result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(6, len); // flag + addr + control + 2 data bytes + flag
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x01, outBuffer[1]); // Address field
    CHECK_EQUAL(0x13, outBuffer[2]); // UI control = 0x03 | P_BIT(0x10) = 0x13 (U-frame gets P bit)
    CHECK_EQUAL(0xDE, outBuffer[3]); // Data byte 1
    CHECK_EQUAL(0xAD, outBuffer[4]); // Data byte 2
    CHECK_EQUAL(0x7E, outBuffer[5]); // Flag
}

// Static callback state for UI receive tests
static bool s_ui_received = false;
static uint8_t s_ui_data[16] = {};
static int s_ui_len = 0;

static void ui_read_callback(void *, uint8_t, uint8_t *buf, int len)
{
    s_ui_received = true;
    s_ui_len = len;
    if ( len > 0 && len <= (int)sizeof(s_ui_data) )
    {
        memcpy(s_ui_data, buf, len);
    }
}

TEST(TINY_FD_ABM, ABM_UIFrameReceiveConnected)
{
    // UI frames can be received when connected
    s_ui_received = false;
    s_ui_len = 0;
    memset(s_ui_data, 0, sizeof(s_ui_data));

    // Close and reinitialize with UI callback
    tiny_fd_close(handle);
    tiny_fd_init_t init{};
    init.pdata = this;
    init.on_connect_event_cb = __onConnect;
    init.on_read_cb = onRead;
    init.on_send_cb = onSend;
    init.log_frame_cb = logFrame;
    init.buffer = inBuffer.data();
    init.buffer_size = inBuffer.size();
    init.window_frames = 7;
    init.send_timeout = 1000;
    init.retry_timeout = 100;
    init.retries = 2;
    init.mode = TINY_FD_MODE_ABM;
    init.peers_count = 1;
    init.crc_type = HDLC_CRC_OFF;
    init.on_read_ui_cb = ui_read_callback;
    auto result = tiny_fd_init(&handle, &init);
    CHECK_EQUAL(TINY_SUCCESS, result);

    establishConnection();

    // Send UI frame: address=0x01, control=0x03 (UI), data=0xBE 0xEF
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x01\x03\xBE\xEF\x7E", 6);
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK(s_ui_received);
    CHECK_EQUAL(2, s_ui_len);
    CHECK_EQUAL(0xBE, s_ui_data[0]);
    CHECK_EQUAL(0xEF, s_ui_data[1]);
}

TEST(TINY_FD_ABM, ABM_UIFrameReceiveDisconnected)
{
    // UI frames can be received even when disconnected (connectionless)
    s_ui_received = false;
    s_ui_len = 0;

    // Close and reinitialize with UI callback
    tiny_fd_close(handle);
    tiny_fd_init_t init{};
    init.pdata = this;
    init.on_connect_event_cb = __onConnect;
    init.on_read_cb = onRead;
    init.on_send_cb = onSend;
    init.log_frame_cb = logFrame;
    init.buffer = inBuffer.data();
    init.buffer_size = inBuffer.size();
    init.window_frames = 7;
    init.send_timeout = 1000;
    init.retry_timeout = 100;
    init.retries = 2;
    init.mode = TINY_FD_MODE_ABM;
    init.peers_count = 1;
    init.crc_type = HDLC_CRC_OFF;
    init.on_read_ui_cb = ui_read_callback;
    auto result = tiny_fd_init(&handle, &init);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Don't establish connection - send UI while disconnected
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x01\x03\x42\x7E", 5);
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK(s_ui_received);
    CHECK_EQUAL(1, s_ui_len);
    CHECK_EQUAL(0x42, s_ui_data[0]);
}

// ==========================================================================
// SREJ (Selective Reject) Tests
// ==========================================================================

TEST(TINY_FD_ABM, ABM_SREJRetransmitsSingleFrame)
{
    establishConnection();
    // Queue one I-frame
    int result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xAA", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);
    // Send the I-frame out
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    uint8_t sent_control = outBuffer[2];
    CHECK_EQUAL(0x00, sent_control & 0x01); // I-frame (bit 0 = 0)
    CHECK_EQUAL(0x00, (sent_control >> 1) & 0x07); // N(S) = 0

    // Peer sends SREJ for frame 0: address=0x03 (command), control=0x0D (SREJ, N(R)=0)
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x0D\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, read_result);

    // Should retransmit frame 0
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    CHECK_EQUAL(0x00, outBuffer[2] & 0x01); // I-frame
    CHECK_EQUAL(0x00, (outBuffer[2] >> 1) & 0x07); // N(S) = 0 (retransmitted)
    CHECK_EQUAL(0xAA, outBuffer[3]); // Original payload preserved
}

TEST(TINY_FD_ABM, ABM_SREJRetransmitsOnlyRequestedFrame)
{
    establishConnection();
    // Queue two I-frames
    int result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xAA", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);
    result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xBB", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Send both I-frames out
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    int len2 = tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 100);
    CHECK(len2 > 0);

    // Peer sends SREJ for frame 0 only: control=0x0D (SREJ, N(R)=0)
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x0D\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, read_result);

    // Get retransmitted frame - should be only frame 0
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    CHECK_EQUAL(0x00, (outBuffer[2] >> 1) & 0x07); // N(S) = 0

    // No more frames should be available (frame 1 was NOT retransmitted)
    len2 = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(0, len2); // Nothing else to send
}

TEST(TINY_FD_ABM, ABM_SREJForSecondFrame)
{
    establishConnection();
    // Queue two I-frames
    int result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xAA", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);
    result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xBB", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Send both I-frames out
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    len += tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 100);

    // Peer acks frame 0, sends SREJ for frame 1: control=0x2D (SREJ, N(R)=1)
    // N(R)=1 means: confirmed frame 0, please retransmit frame 1
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x2D\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, read_result);

    // Get retransmitted frame - should be frame 1 with payload 0xBB
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    CHECK_EQUAL(0x01, (outBuffer[2] >> 1) & 0x07); // N(S) = 1
    CHECK_EQUAL(0xBB, outBuffer[3]); // Frame 1 payload
}

TEST(TINY_FD_ABM, ABM_REJRetransmitsAllFromNR)
{
    // Compare with REJ behavior: REJ retransmits ALL frames from N(R) onward
    establishConnection();
    // Queue two I-frames
    int result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xAA", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);
    result = tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, (const void *)"\xBB", 1, 1000);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Send both I-frames out
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    len += tiny_fd_get_tx_data(handle, outBuffer.data() + len, outBuffer.size() - len, 100);

    // Peer sends REJ for frame 0: control=0x09 (REJ, N(R)=0)
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x03\x09\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, read_result);

    // REJ should retransmit BOTH frames: frame 0 and frame 1
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len > 0);
    CHECK_EQUAL(0x00, (outBuffer[2] >> 1) & 0x07); // N(S) = 0 (first retransmit)

    int len2 = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK(len2 > 0);
    CHECK_EQUAL(0x01, (outBuffer[2] >> 1) & 0x07); // N(S) = 1 (second retransmit)
}