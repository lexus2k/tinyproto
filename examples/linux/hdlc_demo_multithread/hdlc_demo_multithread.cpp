/*
    Copyright 2019-2020 (C) Alexey Dynda

    This file is part of Tiny Protocol Library.

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

/**
 * This is example with single hdlc thread, which shows how to work with TinyProto
 * library in mutilthread mode. This example is suitable for heavy OS like Linux and
 * Windows.
 */

#include "hal/tiny_serial.h"
#include "proto/hdlc/high_level/hdlc.h"
#include "TinyProtocol.h"
#include <stdio.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <queue>

static tiny_mutex_t queue_mutex;
static std::queue<char *> queue{};
static bool peek_next;

static void send_message(const char *message)
{
    tiny_mutex_lock(&queue_mutex);
    char *msg = strdup(message);
    queue.push(msg);
    tiny_mutex_unlock(&queue_mutex);
}

static char *peek_message()
{
    tiny_mutex_lock(&queue_mutex);
    char *msg = nullptr;
    if ( queue.size() )
    {
        msg = queue.front();
        queue.pop();
    }
    tiny_mutex_unlock(&queue_mutex);
    return msg;
}

static int on_frame_read(void *user_data, void *data, int len)
{
    // Process all incoming data here
    fprintf(stderr, "Received message '%.*s'\n", len, (char *)data);
    return 0;
}

static int on_frame_send(void *user_data, const void *data, int len)
{
    // This callback is called, when frame is sent
    fprintf(stderr, "Sent message '%.*s'\n", len, (char *)data);
    peek_next = true;
    return 0;
}

static void protocol_tx_thread(tiny_serial_handle_t serial, hdlc_handle_t handle)
{
    uint8_t tx[4];
    int tx_len = 0;
    int tx_pos = 0;

    // run infinite loop
    char *message = nullptr;
    peek_next = true;
    for ( ;; )
    {
        if ( peek_next )
        {
            if ( message )
                free(message);
            message = peek_message();
            if ( message )
            {
                hdlc_send(handle, message, static_cast<int>(strlen(message)), 0);
                peek_next = false;
            }
        }
        if ( !tx_len )
        {
            tx_pos = 0;
            tx_len = hdlc_get_tx_data(handle, tx, sizeof(tx));
        }
        if ( tx_len )
        {
            int result = tiny_serial_send_timeout(serial, tx + tx_pos, tx_len, 100);
            if ( result < 0 )
            {
                break;
            }
            tx_pos += result;
            tx_len -= result;
        }
    }
}

static void protocol_rx_thread(tiny_serial_handle_t serial, hdlc_handle_t handle)
{
    uint8_t rx[4];
    int rx_len = 0;
    int rx_pos = 0;

    for ( ;; )
    {
        if ( rx_len == 0 )
        {
            int result = tiny_serial_read_timeout(serial, rx, sizeof(rx), 100);
            if ( result < 0 )
            {
                break;
            }
            rx_len = result;
            rx_pos = 0;
        }
        int result = hdlc_run_rx(handle, rx + rx_pos, rx_len, nullptr);
        rx_pos += result;
        rx_len -= result;
    }
}

int main(int argc, char *argv[])
{
#if defined(__linux__)
    tiny_serial_handle_t serial = tiny_serial_open("/dev/ttyS8", 115200);
#elif defined(_WIN32)
    tiny_serial_handle_t serial = tiny_serial_open("COM8", 115200);
#elif defined(__APPLE__) && defined(__MACH__)
    tiny_serial_handle_t serial = tiny_serial_open("/dev/tty.usbmodem0", 115200);
#endif

    if ( serial == TINY_SERIAL_INVALID )
    {
        fprintf(stderr, "Error opening serial port\n");
        return 1;
    }

    // Init hdlc protocol
    hdlc_struct_t conf{};
    conf.send_tx = nullptr;
    conf.on_frame_read = on_frame_read;
    conf.on_frame_send = on_frame_send;
    conf.rx_buf = malloc(1024);
    conf.rx_buf_size = 1024;
    conf.crc_type = HDLC_CRC_16;
    conf.user_data = nullptr;
    conf.multithread_mode = 1;

    hdlc_handle_t handle = hdlc_init(&conf);
    if ( !handle )
    {
        tiny_serial_close(serial);
        fprintf(stderr, "Error initializing hdlc protocol\n");
        return 1;
    }

    tiny_mutex_create(&queue_mutex);
    std::thread tx_thread(protocol_tx_thread, serial, handle);
    std::thread rx_thread(protocol_rx_thread, serial, handle);
    // Main program cycle
    for ( ;; )
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        send_message("Hello message");
    }

    tx_thread.join();
    rx_thread.join();

    hdlc_close(handle);
    free(conf.rx_buf);

    tiny_mutex_destroy(&queue_mutex);
    tiny_serial_close(serial);
    return 0;
}
