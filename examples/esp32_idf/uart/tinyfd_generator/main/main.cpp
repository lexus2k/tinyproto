/*
    MIT License

    Copyright (c) 2020, Alexey Dynda

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <TinyProtocolFd.h>
#include <chrono>
#include <stdio.h>

#define TINY_MULTITHREAD
#define WINDOW_SIZE (7)
#define GENERATED_PACKET_SIZE (64)
#define BUF_SIZE (512)

/* Creating protocol object is simple. Lets define 256 bytes as maximum. *
 * size for the packet and use 7 packets in outgoing queue.             */
Tiny::ProtoFdD proto( tiny_fd_buffer_size_by_mtu( 256, WINDOW_SIZE ) );

uint32_t s_receivedBytes = 0;
uint32_t s_receivedOverheadBytes = 0;
uint32_t s_sentBytes = 0;
uint32_t s_sentOverheadBytes = 0;

void onReceive(void *_, Tiny::IPacket &pkt)
{
    s_receivedBytes += pkt.size();
    s_receivedOverheadBytes += /* ESCAPE */ 2 + /* CRC16 */ 2 + /* I header */ 2 ;
}

void onSendFrameFd(void *_, Tiny::IPacket &pkt)
{
    s_sentBytes += pkt.size();
    s_sentOverheadBytes += /* ESCAPE */ 2 + /* CRC16 */ 2 + /* I header */ 2 ;
}

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#if defined(TINY_MULTITHREAD)
void tx_task(void *arg)
{
    for (;;)
    {
        proto.run_tx( 100 );
    }
    vTaskDelete( NULL );
}

void rx_task(void *arg)
{
    for (;;)
    {
        proto.run_rx( 100 );
    }
    vTaskDelete( NULL );
}

#endif

void main_task(void *args)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick = false,
//        .source_clk = 0, // APB
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, GPIO_NUM_4 /*TX*/, GPIO_NUM_5 /* RX */, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0));

    /* Lets use 16-bit checksum as ESP32 allows that */
    proto.enableCrc16();
    /* Lets use 7 frames window for outgoing messages */
    proto.setWindowSize( WINDOW_SIZE );
    proto.setReceiveCallback( onReceive );
    proto.setSendCallback( onSendFrameFd );
#if defined(TINY_MULTITHREAD)
    proto.setSendTimeout( 100 );
#endif
    /* Redirect all protocol communication to Serial0 UART */
#if defined(TINY_MULTITHREAD)
    proto.begin([](void *p, const void *b, int s)->int { return uart_write_bytes(UART_NUM_1, (const char *)b, s); },
                [](void *p, void *b, int s)->int { return uart_read_bytes(UART_NUM_1, (uint8_t *)b, s, 10); });
#else
    proto.begin([](void *p, const void *b, int s)->int { return uart_tx_chars(UART_NUM_1, (const char *)b, s); },
                [](void *p, void *b, int s)->int { return uart_read_bytes(UART_NUM_1, (uint8_t *)b, s, 0); });
#endif

#if defined(TINY_MULTITHREAD)
    xTaskCreate( tx_task, "tx_task", 2096, NULL, 1, NULL );
    xTaskCreate( rx_task, "rx_task", 2096, NULL, 1, NULL );
#endif
    auto startTs = std::chrono::steady_clock::now();

    Tiny::PacketD packet(GENERATED_PACKET_SIZE);
    packet.put("Generated frame. test in progress");

    for(;;)
    {

        if ( proto.write( packet.data(), packet.size() ) < 0 )
        {
//            fprintf( stderr, "Failed to send packet\n" );
        }
        auto ts = std::chrono::steady_clock::now();
        if (ts - startTs >= std::chrono::seconds(5))
        {
            startTs = ts;
            fprintf(stderr, ".");
            printf("\nRegistered TX speed: payload %u bps, total %u bps\n", (s_sentBytes) * 8 / 5,
                                                                            (s_sentBytes + s_sentOverheadBytes) * 10 / 5);
            printf("Registered RX speed: payload %u bps, total %u bps\n",   (s_receivedBytes) * 8 / 5,
                                                                            (s_receivedBytes + s_receivedOverheadBytes) * 10 / 5);
            s_sentBytes = 0;
            s_receivedBytes = 0;
            s_receivedOverheadBytes = 0;
            s_sentOverheadBytes = 0;
        }
#if defined(TINY_MULTITHREAD)

#else
        proto.run_rx();
        proto.run_tx();
#endif
    }
    vTaskDelete( NULL );
}

extern "C" void app_main(void)
{
    xTaskCreatePinnedToCore(main_task, "mainTask", 8192, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
}
