/*
    Copyright 2017-2024 (C) Alexey Dynda

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

/**
 This is UART support implementation.

 @file
 @brief Tiny UART serial API
*/

#pragma once

#ifdef __cplusplus

extern "C"
{
#endif

    /**
     * @ingroup SERIAL
     * @{
     */

#include <stdint.h>
#if defined(ARDUINO) || defined(__AVR__)
#include "arduino/arduino_serial.h"
#elif defined(__linux__)
#include "linux/linux_serial.h"
#elif defined(__APPLE__) && defined(__MACH__)
#include "macos/macos_serial.h"
#elif defined(_WIN32)
#include "win32/win32_serial.h"
#elif defined(__XTENSA__)
#include "esp32/esp32_serial.h"
#else
#include "no_platform/noplatform_serial.h"
#endif

    /**
     * @brief Opens serial port
     *
     * Opens serial port by name
     *
     * @param name path to the port to open
     *             For linux this can be "/dev/ttyO1", "/dev/ttyS1", "/dev/ttyUSB0", etc.
     *             For windows this can be "COM1", "COM2", etc.
     *             For ESP32 IDF the line should in format "uart0,tx,rx,rts,cts", where
     *                   uart0 - uart hardware device number ("uart1", "uart2"); "tx", "rx",
     *                   "rts", "cts" are optional arguments, specifying integer pin numbers (-1 to use
     *                   standard pin).
     *             For Arduino this is must be pointer to HardwareSerial class
     * @param baud baud rate in bits
     * @return valid serial handle or TINY_SERIAL_INVALID in case of error
     */
    extern tiny_serial_handle_t tiny_serial_open(const char *name, uint32_t baud);

    /**
     * @brief Closes serial connection
     *
     * Closes serial connection
     * @param port handle to serial port
     */
    extern void tiny_serial_close(tiny_serial_handle_t port);

    /**
     * @brief Sends data over serial connection
     *
     * Sends data over serial connection with 100 ms timeout.
     * @param port handle to serial port
     * @param buf pointer to data buffer to send
     * @param len length of data to send
     * @return negative value in case of error.
     *         or number of bytes sent
     */
    extern int tiny_serial_send(tiny_serial_handle_t port, const void *buf, int len);

    /**
     * @brief Sends data over serial connection
     *
     * Sends data over serial connection.
     * @param port handle to serial port
     * @param buf pointer to data buffer to send
     * @param len length of data to send
     * @param timeout_ms timeout in milliseconds to wait until data are sent
     * @return negative value in case of error.
     *         or number of bytes sent
     */
    extern int tiny_serial_send_timeout(tiny_serial_handle_t port, const void *buf, int len, uint32_t timeout_ms);

    /**
     * @brief Receive data from serial connection
     *
     * Receive data from serial connection with 100ms timeout.
     * @param port handle to serial port
     * @param buf pointer to data buffer to read to
     * @param len maximum size of receive buffer
     * @return negative value in case of error.
     *         or number of bytes received
     */
    extern int tiny_serial_read(tiny_serial_handle_t port, void *buf, int len);

    /**
     * @brief Receive data from serial connection
     *
     * Receive data from serial connection.
     * @param port handle to serial port
     * @param buf pointer to data buffer to read to
     * @param len maximum size of receive buffer
     * @param timeout_ms timeout in milliseconds to wait for incoming data
     * @return negative value in case of error.
     *         or number of bytes received
     */
    extern int tiny_serial_read_timeout(tiny_serial_handle_t port, void *buf, int len, uint32_t timeout_ms);

    /**
     * @}
     */

#ifdef __cplusplus
}
#endif
