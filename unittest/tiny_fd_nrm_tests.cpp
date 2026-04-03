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

TEST_GROUP(TINY_FD_NRM)
{
    void setup()
    {
        connected = 0;
        rxLen = 0;
        rxCount = 0;
        memset(rxData, 0, sizeof(rxData));
        tiny_fd_init_t init{};
        init.pdata = this;
        init.addr = TINY_FD_PRIMARY_ADDR; // Primary station address
        init.peers_count = 2;
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
        init.mode = TINY_FD_MODE_NRM;
        init.crc_type = HDLC_CRC_OFF;
        auto result = tiny_fd_init(&handle, &init);
        CHECK_EQUAL(TINY_SUCCESS, result);
    }

    void teardown()
    {
        tiny_fd_close(handle);
        logFrameFunc = nullptr;
    }

    void onConnect(uint8_t, bool status) { connected = connected + (status ? 1: -1); }
    void onRead(uint8_t, uint8_t *buf, int len) {
        if ( len > 0 && rxLen + len <= (int)sizeof(rxData) ) {
            memcpy(rxData + rxLen, buf, len);
            rxLen += len;
        }
        rxCount++;
    }
    void onSend(uint8_t, const uint8_t *, int) { }

    static void __onConnect(void *udata, uint8_t address, bool connected)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_NRM *>(udata);
        self->onConnect(address, connected);
    }

    static void onRead(void *udata, uint8_t address, uint8_t *buf, int len)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_NRM *>(udata);
        self->onRead(address, buf, len);
    }

    static void onSend(void *udata, uint8_t address, const uint8_t *buf, int len)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_NRM *>(udata);
        self->onSend(address, buf, len);
    }

    static void logFrame(void *udata, tiny_fd_handle_t handle, tiny_fd_frame_direction_t direction,
                        tiny_fd_frame_type_t frame_type, tiny_fd_frame_subtype_t frame_subtype,
                        uint8_t ns, uint8_t nr, const uint8_t *data, int len)
    {
        // get the instance of the test class
        auto *self = static_cast<TEST_GROUP_CppUTestGroupTINY_FD_NRM *>(udata);
        if (!self->logFrameFunc) {
            return; // No logging function set
        }
        self->logFrameFunc(handle, direction, frame_type, frame_subtype, ns, nr, data, len);
    }

    tiny_fd_handle_t handle = nullptr;
    int connected = 0;
    uint8_t rxData[256]{};
    int rxLen = 0;
    int rxCount = 0;
    std::array<uint8_t, 1024> inBuffer{};
    std::array<uint8_t, 1024> outBuffer{};
    std::function<void(tiny_fd_handle_t, tiny_fd_frame_direction_t,
                       tiny_fd_frame_type_t, tiny_fd_frame_subtype_t, uint8_t, uint8_t,
                       const uint8_t *, int)> logFrameFunc = nullptr;

    void establishConnection(uint8_t addr)
    {
        // We use emulatation of SNRM frame coming from secondary station
        // We force primary station to receive a token from secondary station: 0x3F
        // This will allow primary station to respond immediately with UA frame
        // FD protocol engine must be updated to handle U and I frame separately for each peer
        const uint8_t snrm_frame[] = {0x7E, (uint8_t)(0x01 | (addr << 2)), 0x3F, 0x7E}; // SNRM frame
        auto read_result = tiny_fd_on_rx_data(handle, snrm_frame, sizeof(snrm_frame));
        CHECK_EQUAL(TINY_SUCCESS, read_result);
        int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
        CHECK_EQUAL(4, len);
        // Check UA frame
        CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
        CHECK_EQUAL(0x01 | (addr << 2), outBuffer[1]); // Address field
        CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
        CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    }
};

TEST(TINY_FD_NRM, NRM_ConnectionInitiatedFromPrimary)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check SNRM frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x07, outBuffer[1]); // Address field: 0x01 peer, CR bit set
    CHECK_EQUAL(0x93, outBuffer[2]); // SNRM packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    // Now emulate answer from secondary station
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x07\x73\x7E", 4); // UA frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK_EQUAL(1, connected); // Connection should be established

    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check SNRM frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x0B, outBuffer[1]); // Address field: 0x02 peer, CR bit set
    CHECK_EQUAL(0x93, outBuffer[2]); // SNRM packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    // Now emulate answer from secondary station
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x0B\x73\x7E", 4); // UA frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK_EQUAL(2, connected); // Connection should be established
}

TEST(TINY_FD_NRM, NRM_ConnectInitiatedFromSecondary)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    // For command requests CR bit must be set, that is why address is 0x03
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x07\x2F\x7E", 4); // SNRM frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check UA frame
    // UA frame should be: 0x7E 0x03 0x00 0x7E
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x05, outBuffer[1]); // Address field
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    CHECK_EQUAL(1, connected); // Connection should be established
}

TEST(TINY_FD_NRM, NRM_ConnectionWhenNoSecondaryStationIsRegistered)
{
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(TINY_ERR_UNKNOWN_PEER, len);
}

TEST(TINY_FD_NRM, NRM_CheckUnitTestConnectionLogicForPrimary)
{
    // This test is to check that connection logic works correctly
    // when we are in NRM mode and no secondary station is registered.
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    CHECK_EQUAL(1, connected); // Connection should be established
    establishConnection(0x02);
    CHECK_EQUAL(2, connected); // Connection should be established
}

TEST(TINY_FD_NRM, NRM_SecondaryDisconnection)
{
    tiny_fd_register_peer(handle, 0x01);
    establishConnection(0x01);
    CHECK_EQUAL(1, connected); // Connection should be established
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x07\x53\x7E", 4); // DISC frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check UA frame   
    // UA frame should be: 0x7E 0x03 0x00 0x7E
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x05, outBuffer[1]); // address field - CR bit must be cleared
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    CHECK_EQUAL(0, connected); // Connection should be closed
}

TEST(TINY_FD_NRM, NRM_DisconnectedState_PrimaryIgnoresAllFramesExcept_SNRM)
{
    tiny_fd_register_peer(handle, 0x01);
    // Send UA frame from secondary station to primary station
    // This should not change the connection state, because primary station is not connected
    auto read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x73\x7E", 4); // UA frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK_EQUAL(0, connected); // Connection should not be established

    // Check that unknown source is ignored
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x25\x93\x7E", 4); // Unknown frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    CHECK_EQUAL(0, connected); // Connection should not be established

    // Now send SNRM frame from secondary station to primary station (CR bit set = command)
    read_result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x07\x93\x7E", 4); // SNRM frame
    CHECK_EQUAL(TINY_SUCCESS, read_result);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    // Check UA frame
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x05, outBuffer[1]); // Address field (response, no CR)
    CHECK_EQUAL(0x73, outBuffer[2]); // UA packet with P/F (NRM always sets P/F)
    CHECK_EQUAL(0x7E, outBuffer[3]); // Flag
    CHECK_EQUAL(1, connected); // Connection should be established
}

TEST(TINY_FD_NRM, NRM_PrimaryToSecondary_SingleIFrame)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);
    // After establishing both, next_peer=0, marker released
    // Give marker back by injecting RR+F from peer 1
    auto result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x11\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Queue 1 I-frame for peer 1
    uint8_t payload[] = {'A'};
    result = tiny_fd_send_to(handle, 0x01, payload, 1, 100);
    CHECK(result > 0);

    // Get TX data — single I-frame with P bit (last and only frame)
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x7E, outBuffer[0]); // Flag
    CHECK_EQUAL(0x05, outBuffer[1]); // Address (peer 1, no CR on I-frames)
    CHECK_EQUAL(0x10, outBuffer[2]); // I(0,0) with P bit
    CHECK_EQUAL('A', outBuffer[3]);  // Payload
    CHECK_EQUAL(0x7E, outBuffer[4]); // Flag
}

TEST(TINY_FD_NRM, NRM_PrimaryToSecondary_MultiplIFrames_PFOnLastOnly)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);
    // Give marker back
    auto result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x11\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Queue 2 I-frames for peer 1
    uint8_t data1[] = {'A'};
    uint8_t data2[] = {'B'};
    result = tiny_fd_send_to(handle, 0x01, data1, 1, 100);
    CHECK(result > 0);
    result = tiny_fd_send_to(handle, 0x01, data2, 1, 100);
    CHECK(result > 0);

    // First call: first I-frame WITHOUT P bit (more frames pending)
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x05, outBuffer[1]); // Address
    CHECK_EQUAL(0x00, outBuffer[2]); // I(0,0) NO P bit
    CHECK_EQUAL('A', outBuffer[3]);
    CHECK_EQUAL(0x7E, outBuffer[4]);

    // Second call: second I-frame WITH P bit (last frame)
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x05, outBuffer[1]); // Address
    CHECK_EQUAL(0x12, outBuffer[2]); // I(1,0) WITH P bit
    CHECK_EQUAL('B', outBuffer[3]);
    CHECK_EQUAL(0x7E, outBuffer[4]);
}

TEST(TINY_FD_NRM, NRM_SecondaryToRrimary_IFrameReceive)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);

    // Inject I-frame+F from peer 1: N(S)=0, N(R)=0, F=1, payload='X'
    // Address 0x05 (no CR = response from secondary), control 0x10 (I(0,0)+F)
    const uint8_t iframe[] = {0x7E, 0x05, 0x10, 'X', 0x7E};
    auto result = tiny_fd_on_rx_data(handle, iframe, sizeof(iframe));
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Verify data received via callback
    CHECK_EQUAL(1, rxCount);
    CHECK_EQUAL(1, rxLen);
    CHECK_EQUAL('X', rxData[0]);

    // Primary should send RR to acknowledge (N(R)=1, P/F bit set)
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x05, outBuffer[1]); // Address (no CR on response RR)
    CHECK_EQUAL(0x31, outBuffer[2]); // RR, N(R)=1, P/F=1 (S-frame always gets P/F)
    CHECK_EQUAL(0x7E, outBuffer[3]);
}

TEST(TINY_FD_NRM, NRM_MarkerTimeoutRecovery)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);
    // Marker released after connection. Don't inject any response.
    // Wait for retry_timeout (100ms) — tiny_fd_get_tx_data waits internally
    // First call: waits for marker, times out, reclaims marker, queues idle RR
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    // May return 0 (marker reclaimed but frame not sent yet) or >0
    if ( len == 0 )
    {
        // Second call: sends the queued RR
        len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    }
    CHECK(len > 0);
    // Should be an RR poll or SNRM (depending on peer state after timeout)
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x7E, outBuffer[len - 1]);
}

TEST(TINY_FD_NRM, NRM_PrimaryDisconnect)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);
    CHECK_EQUAL(2, connected);

    // Primary initiates disconnect (currently disconnects peer 0)
    auto result = tiny_fd_disconnect(handle);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Give marker back to send DISC
    result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x11\x7E", 4); // RR+F from peer 1
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Get DISC frame
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x07, outBuffer[1]); // Address with CR bit (DISC is a command)
    CHECK_EQUAL(0x53, outBuffer[2]); // DISC + P/F
    CHECK_EQUAL(0x7E, outBuffer[3]);

    // Secondary responds with UA+F
    result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x73\x7E", 4); // UA from peer 1
    CHECK_EQUAL(TINY_SUCCESS, result);
    CHECK_EQUAL(1, connected); // One connection closed
}

TEST(TINY_FD_NRM, NRM_DMResponseWhenDisconnected)
{
    tiny_fd_register_peer(handle, 0x01);
    // Peer is disconnected. Send a command U-frame (DISC+P with CR bit) from that peer.
    // Primary should respond with DM since peer is disconnected.
    const uint8_t disc_frame[] = {0x7E, 0x07, 0x53, 0x7E}; // DISC+P from peer 1 (CR=command)
    auto result = tiny_fd_on_rx_data(handle, disc_frame, sizeof(disc_frame));
    CHECK_EQUAL(TINY_SUCCESS, result);

    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x05, outBuffer[1]); // Address (response, no CR)
    CHECK_EQUAL(0x1F, outBuffer[2]); // DM + F bit
    CHECK_EQUAL(0x7E, outBuffer[3]);
}

TEST(TINY_FD_NRM, NRM_RoundRobinPeerSwitching)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);
    // After establishing both: next_peer=0, marker released

    // Give marker back
    auto result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x11\x7E", 4); // RR+F from peer 1
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Queue data for both peers
    uint8_t d1[] = {'A'};
    uint8_t d2[] = {'X'};
    result = tiny_fd_send_to(handle, 0x01, d1, 1, 100);
    CHECK(result > 0);
    result = tiny_fd_send_to(handle, 0x02, d2, 1, 100);
    CHECK(result > 0);

    // TX1: I-frame to peer 0 (addr 0x01) with P → marker released, next_peer switches to 1
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x05, outBuffer[1]); // Peer 1 address
    CHECK_EQUAL(0x10, outBuffer[2]); // I(0,0)+P (last frame for this peer)
    CHECK_EQUAL('A', outBuffer[3]);

    // Peer 0 responds with RR+F, N(R)=1 (confirming receipt of I-frame) → marker captured
    result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x31\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // TX2: now next_peer=1, send I-frame to peer 1 (addr 0x02) with P
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x09, outBuffer[1]); // Peer 2 address
    CHECK_EQUAL(0x10, outBuffer[2]); // I(0,0)+P
    CHECK_EQUAL('X', outBuffer[3]);
}

TEST(TINY_FD_NRM, NRM_FRMRRecovery)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);
    CHECK_EQUAL(2, connected);

    // Inject FRMR from peer 1 (with F bit to give marker)
    // FRMR = 0x87 | 0x10 = 0x97, plus 2 info bytes
    const uint8_t frmr[] = {0x7E, 0x05, 0x97, 0x00, 0x00, 0x7E};
    auto result = tiny_fd_on_rx_data(handle, frmr, sizeof(frmr));
    CHECK_EQUAL(TINY_SUCCESS, result);
    // FRMR should cause disconnection and re-connection attempt
    CHECK_EQUAL(1, connected); // peer 1 disconnected

    // Primary should send SNRM (not SABM, since NRM mode) to re-establish
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x7E, outBuffer[0]);
    CHECK_EQUAL(0x07, outBuffer[1]); // Address with CR (SNRM is command)
    CHECK_EQUAL(0x93, outBuffer[2]); // SNRM + P/F
    CHECK_EQUAL(0x7E, outBuffer[3]);

    // Secondary responds with UA
    result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x73\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, result);
    CHECK_EQUAL(2, connected); // Re-connected
}

TEST(TINY_FD_NRM, NRM_REJRecovery)
{
    tiny_fd_register_peer(handle, 0x01);
    tiny_fd_register_peer(handle, 0x02);
    establishConnection(0x01);
    establishConnection(0x02);

    // Give marker and send an I-frame to peer 0 (addr 0x01)
    auto result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x11\x7E", 4); // RR+F
    CHECK_EQUAL(TINY_SUCCESS, result);
    uint8_t d1[] = {'A'};
    result = tiny_fd_send_to(handle, 0x01, d1, 1, 100);
    CHECK(result > 0);
    int len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x10, outBuffer[2]); // I(0,0)+P
    // After sending with P: marker released, next_peer = 1

    // Peer 0 responds with REJ+F, N(R)=0 (requesting retransmission from 0)
    result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x05\x19\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, result);
    // Marker captured, but next_peer = 1 (round-robin)

    // TX for peer 1: idle RR+P (no data for peer 1)
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL(0x09, outBuffer[1]); // Peer 2 address
    // After sending RR+P: marker released, next_peer = 0

    // Peer 1 responds with RR+F → marker captured, next_peer still 0
    result = tiny_fd_on_rx_data(handle, (uint8_t *)"\x7E\x09\x11\x7E", 4);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Now next_peer = 0, and I-frame needs retransmission
    len = tiny_fd_get_tx_data(handle, outBuffer.data(), outBuffer.size(), 100);
    CHECK_EQUAL(5, len);
    CHECK_EQUAL(0x05, outBuffer[1]); // Peer 1 address
    CHECK_EQUAL(0x10, outBuffer[2]); // I(0,0)+P retransmitted
    CHECK_EQUAL('A', outBuffer[3]);
}

