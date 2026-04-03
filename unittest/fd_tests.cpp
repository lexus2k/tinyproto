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
#include "helpers/tiny_fd_helper.h"
#include "helpers/fake_connection.h"

TEST_GROUP(FD)
{
    void setup()
    {
        // ...
        //        fprintf(stderr, "======== START =======\n" );
    }

    void teardown()
    {
                   // ...
                   //        fprintf(stderr, "======== END =======\n" );
    }
};

TEST(FD, multithread_basic_test)
{
    FakeSetup conn;
    uint16_t nsent = 0;
    TinyHelperFd helper1(&conn.endpoint1(), 4096, nullptr, 7, 250);
    TinyHelperFd helper2(&conn.endpoint2(), 4096, nullptr, 7, 250);
    // Vaildation of tiny_fd_run_rx()
    helper1.run(true);
    helper2.run(true);

    // sent 200 small packets
    for ( nsent = 0; nsent < 200; nsent++ )
    {
        uint8_t txbuf[4] = {0xAA, 0xFF, 0xCC, 0x66};
        int result = helper2.send(txbuf, sizeof(txbuf));
        CHECK_EQUAL(TINY_SUCCESS, result);
    }
    // wait until last frame arrives
    helper1.wait_until_rx_count(200, 250);
    CHECK_EQUAL(200, helper1.rx_count());
}

TEST(FD, multithread_read_test)
{
    FakeSetup conn;
    uint16_t nsent = 0;
    TinyHelperFd helper1(&conn.endpoint1(), 4096, nullptr, 7, 250);
    TinyHelperFd helper2(&conn.endpoint2(), 4096, nullptr, 7, 250);
    // Validation of tiny_fd_on_rx_data()
    helper1.run(true);
    helper2.run(true);

    // sent 200 small packets
    for ( nsent = 0; nsent < 200; nsent++ )
    {
        uint8_t txbuf[4] = {0xAA, 0xFF, 0xCC, 0x66};
        int result = helper2.send(txbuf, sizeof(txbuf));
        CHECK_EQUAL(TINY_SUCCESS, result);
    }
    // wait until last frame arrives
    helper1.wait_until_rx_count(200, 250);
    CHECK_EQUAL(200, helper1.rx_count());
}

TEST(FD, arduino_to_pc)
{
    std::atomic<int> arduino_timedout_frames{};
    FakeSetup conn(4096, 64); // PC side has larger UART buffer: 4096, arduino side has small uart buffer
    TinyHelperFd pc(&conn.endpoint1(), 4096, nullptr, 4, 400);
    TinyHelperFd arduino(
        &conn.endpoint2(), tiny_fd_buffer_size_by_mtu(64, 4),
        [&arduino, &arduino_timedout_frames](uint8_t a, uint8_t *b, int s) -> void {
            if ( arduino.send(b, s) == TINY_ERR_TIMEOUT )
                arduino_timedout_frames++;
        },
        4, 0);
    conn.endpoint2().setTimeout(0);
    conn.endpoint2().disable();
    conn.setSpeed(115200);
    pc.run(true);
    // sent 100 small packets from pc to arduino
    pc.send(100, "Generated frame. test in progress");
    // Usually arduino starts later by 2 seconds due to reboot on UART-2-USB access, emulate al teast 100ms delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    conn.endpoint2().enable();
    uint32_t start_ms = tiny_millis();
    do
    {
        arduino.run_rx();
        arduino.run_tx();
        if ( static_cast<uint32_t>(tiny_millis() - start_ms) > 2000 )
            break;
    } while ( pc.tx_count() != 100 && pc.rx_count() + arduino_timedout_frames < 99 );
    // it is allowed to miss several frames due to arduino cycle completes before last messages are delivered
    if ( 95 - arduino_timedout_frames > pc.rx_count() )
    {
        CHECK_EQUAL(100 - arduino_timedout_frames, pc.rx_count());
    }
}

TEST(FD, errors_on_tx_line)
{
    // Limit hardware blocks to 32 byte buffers only.
    // Too large blocks cause much frames to be bufferred, and each restransmission
    // contains errors (because we set error every 200 bytes)
    FakeSetup conn(32, 32);
    uint16_t nsent = 0;
    // Also, to make test logs more clear, we limit window size to 3 frames only.
    // This will give us clear understanding what is happenning when something goes wrong
    TinyHelperFd helper1(&conn.endpoint1(), 1024, nullptr, 3, 400);
    TinyHelperFd helper2(&conn.endpoint2(), 1024, nullptr, 3, 400);
    conn.line2().generate_error_every_n_byte(200);
    helper1.run(true);
    helper2.run(true);

    for ( nsent = 0; nsent < 200; nsent++ )
    {
        uint8_t txbuf[4] = {0xAA, 0xFF, 0xCC, 0x66};
        int result = helper2.send(txbuf, sizeof(txbuf));
        CHECK_EQUAL(TINY_SUCCESS, result);
    }
    // wait until last frame arrives
    helper1.wait_until_rx_count(200, 400);
    CHECK_EQUAL(200, helper1.rx_count());
}

TEST(FD, error_on_single_I_send)
{
    // Each U-frame or S-frame is 6 bytes or more: 7F, ADDR, CTL, FSC16, 7F
    // TX1: U, U, R
    // TX2: U, U, I,
    FakeSetup conn;
    uint16_t nsent = 0;
    TinyHelperFd helper1(&conn.endpoint1(), 4096, nullptr, 7, 1000);
    TinyHelperFd helper2(&conn.endpoint2(), 4096, nullptr, 7, 1000);
    conn.line2().generate_single_error(6 + 6 + 3); // Put error on I-frame
    helper1.run(true);
    helper2.run(true);

    // retransmissiomn must happen very quickly since FD detects out-of-order frames
    for ( nsent = 0; nsent < 2; nsent++ )
    {
        uint8_t txbuf[4] = {0xAA, 0xFF, 0xCC, 0x66};
        int result = helper2.send(txbuf, sizeof(txbuf));
        CHECK_EQUAL(TINY_SUCCESS, result);
    }
    // wait until last frame arrives
    helper1.wait_until_rx_count(2, 500);
    CHECK_EQUAL(2, helper1.rx_count());
}

TEST(FD, error_on_rej)
{
    // Each U-frame or S-frame is 6 bytes or more: 7F, ADDR, CTL, FSC16, 7F
    // TX1: U, U, R
    // TX2: U, U, I,
    FakeSetup conn;
    uint16_t nsent = 0;
    TinyHelperFd helper1(&conn.endpoint1(), 4096, nullptr, 7, 250);
    TinyHelperFd helper2(&conn.endpoint2(), 4096, nullptr, 7, 250);
    conn.line2().generate_single_error(6 + 6 + 4); // Put error on first I-frame
    conn.line1().generate_single_error(6 + 6 + 3); // Put error on S-frame REJ
    helper1.run(true);
    helper2.run(true);

    for ( nsent = 0; nsent < 2; nsent++ )
    {
        uint8_t txbuf[4] = {0xAA, 0xFF, 0xCC, 0x66};
        int result = helper2.send(txbuf, sizeof(txbuf));
        CHECK_EQUAL(TINY_SUCCESS, result);
    }
    // wait until last frame arrives
    helper1.wait_until_rx_count(2, 400);
    CHECK_EQUAL(2, helper1.rx_count());
}

TEST(FD, no_ka_switch_to_disconnected)
{
    FakeSetup conn(32, 32);
    TinyHelperFd helper1(&conn.endpoint1(), 1024, nullptr, 4, 100);
    TinyHelperFd helper2(&conn.endpoint2(), 1024, nullptr, 4, 100);
    conn.endpoint1().setTimeout(30);
    conn.endpoint2().setTimeout(30);
    helper1.set_ka_timeout(50);
    helper2.set_ka_timeout(50);
    helper1.run(true);
    helper2.run(true);

    // Consider FD use keep alive to keep connection during 50 milliseconds
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // Stop remote side, and sleep again for 150 milliseconds, to get disconnected state
    helper2.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // Stop both endpoints to do clean flush
    helper1.stop();
    conn.endpoint1().flush();
    conn.endpoint2().flush();
    // Run local endpoint again.
    helper1.run(true);
    // At this step, we should be in disconnected state, and should see SABM frames
    // sleep for 100 milliseconds to get at least one Keep Alive
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint8_t buffer[32];
    int len = conn.endpoint2().read(buffer, sizeof(buffer));
    const uint8_t sabm_request[] = {0x7E, 0x03, 0x3F, 0x5B, 0xEC, 0x7E};
    if ( (size_t)len < sizeof(sabm_request) )
    {
        CHECK_EQUAL(sizeof(sabm_request), len);
    }
    MEMCMP_EQUAL(sabm_request, buffer, sizeof(sabm_request));
}

TEST(FD, resend_timeout)
{
    FakeSetup conn(128, 128);
    TinyHelperFd helper1(&conn.endpoint1(), 1024, nullptr, 4, 70);
    TinyHelperFd helper2(&conn.endpoint2(), 1024, nullptr, 4, 70);
    conn.endpoint1().setTimeout(30);
    conn.endpoint2().setTimeout(30);
    helper1.run(true);
    helper2.run(true);

    // During 100ms FD must establish connection to remote side
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Stop remote side, and try to send something
    helper2.stop();
    conn.endpoint1().flush();
    conn.endpoint2().flush();
    helper1.send("#");
    std::this_thread::sleep_for(std::chrono::milliseconds(70 * 2 + 100));
    helper1.stop();
    const uint8_t reconnect_dat[] = {0x7E, 0x01, 0x00, '#',  0x89, 0x8F, 0x7E, // 1-st attempt (I-frame, no P bit in ABM)
                                     0x7E, 0x01, 0x00, '#',  0x89, 0x8F, 0x7E, // 2-nd attempt (1st retry)
                                     0x7E, 0x01, 0x00, '#',  0x89, 0x8F, 0x7E, // 3-rd attempt (2nd retry)
                                     0x7E, 0x03, 0x3F, 0x5B, 0xEC, 0x7E};      // Attempt to reconnect (SABM)
    uint8_t buffer[64]{};
    conn.endpoint2().read(buffer, sizeof(buffer));
    MEMCMP_EQUAL(reconnect_dat, buffer, sizeof(reconnect_dat));
}

TEST(FD, singlethread_basic)
{
    // TODO:
    CHECK_EQUAL(0, 0);
}

TEST(FD, connecting_in_different_time)
{
    FakeSetup conn;
    TinyHelperFd helper1(&conn.endpoint1(), 4096, nullptr, 7, 250);
    TinyHelperFd helper2(&conn.endpoint2(), 4096, nullptr, 7, 250);
    helper1.run(true);
    tiny_sleep( 400 );
    helper2.run(true);

    uint8_t txbuf[4] = {0xAA, 0xFF, 0xCC, 0x66};
    helper1.send(txbuf, sizeof(txbuf));
    // wait until last frame arrives
    helper2.wait_until_rx_count(1, 100);
    CHECK_EQUAL(1, helper2.rx_count());
}

TEST(FD, on_connect_callback)
{
    FakeSetup conn;
    bool connected = false;
    TinyHelperFd helper1(&conn.endpoint1(), 4096, nullptr, 7, 100);
    TinyHelperFd helper2(&conn.endpoint2(), 4096, nullptr, 7, 100);
    helper1.set_connect_cb( [&connected](uint8_t addr, bool result) -> void { connected = result; } );

    helper1.run(true);
    helper2.run(true);
    // Give some time to initialize threads
    tiny_sleep( 10 );
    uint8_t data[1] = {0xAA};
    helper1.send( data, sizeof(data) );

    for (int i = 0; i < 1000 && !connected; i++)
    {
        tiny_sleep( 1 );
    }
    CHECK_EQUAL(true, connected);
    helper2.stop();

    for (int i = 0; i < 1000 && connected; i++)
    {
        tiny_sleep( 1 );
    }
    CHECK_EQUAL(false, connected);
}
