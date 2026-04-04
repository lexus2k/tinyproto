/*
    Copyright 2025 (C) Alexey Dynda

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
*/

#include <CppUTest/TestHarness.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <functional>

#include "TinyLightProtocol.h"
#include "TinyProtocolHdlc.h"
#include "TinyProtocolFd.h"
#include "TinyPacket.h"

// ========================= Light Wrapper Tests =========================

static uint8_t s_light_tx_buf[1024];
static int s_light_tx_pos;
static uint8_t s_light_rx_buf[1024];
static int s_light_rx_len;

static int lightWriteCb(void *pdata, const void *buffer, int size)
{
    memcpy(s_light_tx_buf + s_light_tx_pos, buffer, size);
    s_light_tx_pos += size;
    return size;
}

static int lightReadCb(void *pdata, void *buffer, int size)
{
    int to_read = s_light_rx_len < size ? s_light_rx_len : size;
    if ( to_read > 0 )
    {
        memcpy(buffer, s_light_rx_buf, to_read);
        memmove(s_light_rx_buf, s_light_rx_buf + to_read, s_light_rx_len - to_read);
        s_light_rx_len -= to_read;
    }
    return to_read;
}

TEST_GROUP(CPP_LIGHT)
{
    void setup()
    {
        s_light_tx_pos = 0;
        s_light_rx_len = 0;
        memset(s_light_tx_buf, 0, sizeof(s_light_tx_buf));
        memset(s_light_rx_buf, 0, sizeof(s_light_rx_buf));
    }

    void teardown()
    {
    }
};

TEST(CPP_LIGHT, WriteReturnsByteCount)
{
    tinyproto::Light light;
    light.disableCrc();
    light.begin(lightWriteCb, lightReadCb);

    char data[] = "Hello";
    int result = light.write(data, strlen(data) + 1);
    CHECK(result > 0);

    light.end();
}

TEST(CPP_LIGHT, WritePacketReturnsByteCount)
{
    tinyproto::Light light;
    light.disableCrc();
    light.begin(lightWriteCb, lightReadCb);

    char buf[64];
    tinyproto::IPacket pkt(buf, sizeof(buf));
    pkt.put("Test");

    // After fix: write(IPacket) returns actual byte count, not bool
    int result = light.write(pkt);
    CHECK(result > 1);  // Must be more than 1 (was 1 when converted to bool)

    light.end();
}

TEST(CPP_LIGHT, SendAndReceive)
{
    tinyproto::Light sender;
    tinyproto::Light receiver;
    sender.disableCrc();
    receiver.disableCrc();

    sender.begin(lightWriteCb, lightReadCb);

    char tx_data[] = "Hello Light";
    int sent = sender.write(tx_data, strlen(tx_data) + 1);
    CHECK(sent > 0);

    // Feed transmitted data into receiver's rx buffer
    memcpy(s_light_rx_buf, s_light_tx_buf, s_light_tx_pos);
    s_light_rx_len = s_light_tx_pos;

    receiver.begin(lightWriteCb, lightReadCb);
    char rx_data[64]{};
    int received = receiver.read(rx_data, sizeof(rx_data));
    CHECK(received > 0);
    STRCMP_EQUAL(tx_data, rx_data);

    sender.end();
    receiver.end();
}

// ========================= Hdlc Wrapper Tests =========================

TEST_GROUP(CPP_HDLC)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(CPP_HDLC, WriteAndGenerateTxData)
{
    uint8_t buffer[1024];
    tinyproto::Hdlc hdlc(buffer, sizeof(buffer));
    hdlc.disableCrc();
    hdlc.begin();

    char data[] = "HDLC test";
    int result = hdlc.write(data, strlen(data) + 1);
    CHECK_EQUAL(TINY_SUCCESS, result);

    // Generate tx data
    uint8_t tx_buf[256];
    int tx_len = hdlc.run_tx(tx_buf, sizeof(tx_buf));
    CHECK(tx_len > 0);

    hdlc.end();
}

TEST(CPP_HDLC, SendAndReceiveFrame)
{
    uint8_t buf1[1024];
    uint8_t buf2[1024];

    // Receiver callback
    static char received_data[256];
    static int received_len;
    received_len = 0;

    class TestHdlc: public tinyproto::Hdlc
    {
    public:
        TestHdlc(uint8_t *buf, int size)
            : Hdlc(buf, size)
        {
        }
        void onReceive(uint8_t *data, int size) override
        {
            memcpy(received_data, data, size);
            received_len = size;
        }
    };

    tinyproto::Hdlc sender(buf1, sizeof(buf1));
    TestHdlc receiver(buf2, sizeof(buf2));
    sender.disableCrc();
    receiver.disableCrc();
    sender.begin();
    receiver.begin();

    char data[] = "Hello HDLC";
    sender.write(data, strlen(data) + 1);

    // Transfer: generate tx bytes from sender, feed to receiver
    uint8_t wire[512];
    int wire_len = sender.run_tx(wire, sizeof(wire));
    CHECK(wire_len > 0);

    int processed = receiver.run_rx(wire, wire_len);
    CHECK(processed > 0);

    CHECK_EQUAL((int)(strlen(data) + 1), received_len);
    STRCMP_EQUAL(data, received_data);

    sender.end();
    receiver.end();
}

TEST(CPP_HDLC, WritePacketInterface)
{
    uint8_t buffer[1024];
    tinyproto::Hdlc hdlc(buffer, sizeof(buffer));
    hdlc.disableCrc();
    hdlc.begin();

    char pkt_buf[64];
    tinyproto::IPacket pkt(pkt_buf, sizeof(pkt_buf));
    pkt.put("PKT");

    int result = hdlc.write(pkt);
    CHECK_EQUAL(TINY_SUCCESS, result);

    hdlc.end();
}

// ========================= FD Wrapper Tests =========================

TEST_GROUP(CPP_FD)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(CPP_FD, BeginAndEnd)
{
    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(2);
    fd.begin();

    CHECK(fd.getHandle() != nullptr);

    fd.end();
}

TEST(CPP_FD, StaticBufferVariant)
{
    tinyproto::Fd<1024> fd;
    fd.disableCrc();
    fd.setWindowSize(2);
    fd.begin();

    CHECK(fd.getHandle() != nullptr);

    fd.end();
}

TEST(CPP_FD, DynamicAllocationCleanup)
{
    // Test that FdD properly allocates and destroys without leak/crash
    {
        tinyproto::FdD fd(2048);
        fd.disableCrc();
        fd.begin();
        fd.end();
    }
    // If we get here without crash, the delete[] type mismatch fix works
    CHECK(true);
}

TEST(CPP_FD, WriteReturnsProperValues)
{
    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(3);
    fd.setSendTimeout(100);
    fd.begin();

    // Send without a connected peer — should fail or timeout
    char data[] = "test data";
    int result = fd.write(data, strlen(data) + 1);
    // Not connected, so send should fail (negative or zero)
    CHECK(result <= 0);

    fd.end();
}

TEST(CPP_FD, RunRxWithDirectData)
{
    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(2);
    fd.begin();

    // Feed some garbage data — should not crash
    uint8_t garbage[] = {0x7E, 0x00, 0x01, 0x7E};
    int result = fd.run_rx(garbage, sizeof(garbage));
    CHECK_EQUAL(TINY_SUCCESS, result);

    fd.end();
}

TEST(CPP_FD, RunTxGeneratesData)
{
    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(2);
    fd.begin();

    // FD protocol generates supervisory frames (SABM etc.)
    uint8_t tx_buf[256];
    int tx_len = fd.run_tx(tx_buf, sizeof(tx_buf));
    // May generate connection setup frames
    CHECK(tx_len >= 0);

    fd.end();
}

TEST(CPP_FD, RunTxCallbackBreaksOnZeroWrite)
{
    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(2);
    fd.begin();

    // Write callback that returns 0 (device busy) — should NOT infinite loop
    static int call_count;
    call_count = 0;
    auto write_func = [](void *pdata, const void *buf, int size) -> int {
        call_count++;
        return 0;  // Simulate device busy
    };

    int result = fd.run_tx(write_func);
    // Should return error or 0, not hang
    CHECK(result <= 0);
    // Should have been called at most once (broke out immediately)
    CHECK(call_count <= 1);

    fd.end();
}

TEST(CPP_FD, ReceiveCallback)
{
    static uint8_t rx_data[256];
    static int rx_len;
    static uint8_t rx_addr;
    rx_len = 0;
    rx_addr = 0;

    auto onRx = [](void *userData, uint8_t addr, tinyproto::IPacket &pkt) {
        memcpy(rx_data, pkt.data(), pkt.size());
        rx_len = pkt.size();
        rx_addr = addr;
    };

    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(3);
    fd.setReceiveCallback(onRx);
    fd.begin();

    // Verify callback was set (no crash)
    CHECK(fd.getHandle() != nullptr);
    CHECK_EQUAL(0, rx_len);
    CHECK_EQUAL(0, rx_addr);

    fd.end();
}

TEST(CPP_FD, ConnectEventCallback)
{
    static bool connected = false;
    static uint8_t event_addr = 0;

    auto onConnect = [](void *userData, uint8_t addr, bool conn) {
        connected = conn;
        event_addr = addr;
    };

    tinyproto::FdD fd(1024);
    fd.disableCrc();
    fd.setWindowSize(2);
    fd.setConnectEventCallback(onConnect);
    fd.begin();

    CHECK(fd.getHandle() != nullptr);
    CHECK_EQUAL(false, connected);
    CHECK_EQUAL(0, event_addr);

    fd.end();
}

TEST(CPP_FD, CrcModes)
{
    tinyproto::FdD fd(2048);
    CHECK(fd.enableCheckSum());
    CHECK(fd.enableCrc16());
    CHECK(fd.enableCrc32());
    fd.disableCrc();
    fd.begin();
    fd.end();
}

// ========================= IPacket Tests =========================

TEST_GROUP(CPP_PACKET)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

TEST(CPP_PACKET, PutAndRead)
{
    char buf[64];
    tinyproto::IPacket pkt(buf, sizeof(buf));

    pkt.put((uint8_t)0x42);
    pkt.put((uint8_t)0x43);

    CHECK_EQUAL(2, pkt.size());
    CHECK_EQUAL(0x42, (uint8_t)buf[0]);
    CHECK_EQUAL(0x43, (uint8_t)buf[1]);
}

TEST(CPP_PACKET, CopyConstructor)
{
    char buf[64];
    tinyproto::IPacket original(buf, sizeof(buf));
    original.put("data");

    tinyproto::IPacket copy(original);
    CHECK_EQUAL(original.size(), copy.size());
    CHECK_EQUAL(original.maxSize(), copy.maxSize());
    CHECK_EQUAL(original.data(), copy.data());
}

TEST(CPP_PACKET, Clear)
{
    char buf[64];
    tinyproto::IPacket pkt(buf, sizeof(buf));
    pkt.put("data");
    CHECK_EQUAL(5, pkt.size());  // "data" + null terminator

    pkt.clear();
    CHECK_EQUAL(0, pkt.size());
}
