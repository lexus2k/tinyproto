/*
    Copyright 2016-2020 (C) Alexey Dynda

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
 This is Tiny protocol implementation for microcontrollers

 @file
 @brief Tiny protocol Types
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__AVR__)
#include "include/avr_hal.h"
#elif defined(__XTENSA__)
#include "include/esp32_hal.h"
#elif defined(ARDUINO)
#include "include/arduino_hal.h"
#elif defined(__linux__)
#include "include/linux_hal.h"
#elif defined(__MINGW32__)
#include "include/mingw32_hal.h"
#elif defined(_WIN32)
#include "include/win32_hal.h"
#else
#info "Platform not supported. Multithread support is disabled"
#include "include/no_platform_hal.h"
#endif

#include <stdint.h>

/// \cond
#ifdef CONFIG_ENABLE_FCS32
    typedef uint32_t fcs_t;
#else
    typedef uint16_t fcs_t;
#endif
/// \endcond

/**
 * @defgroup ERROR_FLAGS Return error codes for Tiny API functions
 * @{
 */
/// Tiny operation successful. Only tiny_send_start and tiny_read_start functions return this code
#define TINY_SUCCESS                    (0)
/// Timeout
#define TINY_ERR_FAILED                 (-1)
/// Timeout happened. The function must be called once again.
#define TINY_ERR_TIMEOUT                (-2)
/// Data too large to fit the user buffer
#define TINY_ERR_DATA_TOO_LARGE         (-3)
/// Some invalid data passed to Tiny API function.
#define TINY_ERR_INVALID_DATA           (-4)
/// API function detected that operation cannot be performed right now.
#define TINY_ERR_BUSY                   (-5)
/// Out of sync - received some data, which are not part of the frame (tiny_read)
#define TINY_ERR_OUT_OF_SYNC            (-6)
/// No data for now, need to retry reading once again
#define TINY_ERR_AGAIN                  (-7)
/// Invalid crc field of incoming frame
#define TINY_ERR_WRONG_CRC              (-8)

/** @} */

/**
 * @defgroup FLAGS_GROUP Flags for Tiny API functions
 * @{
 */

/// This flag makes tiny API functions perform as non-blocking
#define TINY_FLAG_NO_WAIT               (0)
/// This flag makes tiny_read function to read whole frame event if it doesn't fit the buffer
#define TINY_FLAG_READ_ALL              (1)
/// Informs advanced API that caller wants to start transmit new frame to the channel
#define TINY_FLAG_LOCK_SEND             (2)
/// This flag makes tiny API functions perform in blocking mode
#define TINY_FLAG_WAIT_FOREVER          (0x80)

/** @} */

/**
 * The function writes data to communication channel port.
 * @param pdata - pointer to user private data - absent in Arduino version
 * @param buffer - pointer to the data to send to channel.
 * @param size - size of data to write.
 * @see read_block_cb_t
 * @return the function must return negative value in case of error or number of bytes written
 *         or zero.
 */
typedef int (*write_block_cb_t)(void *pdata, const void *buffer, int size);

/**
 * The function reads data from communication channel.
 * @param pdata - pointer to user private data. - absent in Arduino version
 * @param buffer - pointer to a buffer to read data to from the channel.
 * @param size - maximum size of the buffer.
 * @see write_block_cb_t
 * @return the function must return negative value in case of error or number of bytes actually read
 *         or zero.
 */
typedef int (*read_block_cb_t)(void *pdata, void *buffer, int size);


/**
 * on_frame_cb_t is a callback function, which is called every time new frame is received, or sent.
 * refer to tiny_set_callbacks
 * @param handle - handle of Tiny.
 * @param uid    - UID of the received frame or sent frame (if uids are enabled).
 * @param pdata  - data received over Tiny Protocol.
 * @param size   - size of data received.
 * @return None.
 * @see   tiny_set_callbacks
 */
typedef void (*on_frame_cb_t)(void *handle, uint16_t uid, uint8_t *pdata, int size);

#define EVENT_BITS_ALL   0xFF ///< All bits supported by tiny protocol HAL events
#define EVENT_BITS_CLEAR 1    ///< Flag, used in tiny_events_wait()
#define EVENT_BITS_LEAVE 0    ///< Flag, used in tiny_events_wait()

/**
 * Creates cross-platform mutex.
 * @param mutex pointer to tiny_mutex_t variable.
 */
void tiny_mutex_create(tiny_mutex_t *mutex);

/**
 * Destroys cross-platform mutex.
 * @param mutex pointer to tiny_mutex_t variable.
 */
void tiny_mutex_destroy(tiny_mutex_t *mutex);

/**
 * Locks cross-platform mutex.
 * @param mutex pointer to tiny_mutex_t variable.
 */
void tiny_mutex_lock(tiny_mutex_t *mutex);

/**
 * Attempts to lock cross-platform mutex.
 * @param mutex pointer to tiny_mutex_t variable.
 * @return 0 if failed to lock
 *         1 if success
 */
uint8_t tiny_mutex_try_lock(tiny_mutex_t *mutex);

/**
 * Unlocks cross-platform mutex.
 * @param mutex pointer to tiny_mutex_t variable.
 */
void tiny_mutex_unlock(tiny_mutex_t *mutex);

/**
 * Creates cross platform event group object.
 * @param events pointer to tiny_event_t variable.
 */
void tiny_events_create(tiny_events_t *events);

/**
 * Destroys cross platform event group object.
 * @param events pointer to tiny_event_t variable.
 */
void tiny_events_destroy(tiny_events_t *events);

/**
 * Waits until any of specified bits is set or timeout.
 * @param event pointer to tiny_event_t variable.
 * @param bits bits to wait for
 * @param clear flags EVENT_BITS_CLEAR or EVENT_BITS_LEAVE
 * @param timeout timeout in milliseconds to wait
 * @return 0 on timeout
 *         list of bits from the input argument, which were set
 */
uint8_t tiny_events_wait(tiny_events_t *event, uint8_t bits,
                         uint8_t clear, uint32_t timeout);

/**
 * Checks specified bits.
 * This API can be used in interrupt handlers
 * @param event pointer to tiny_event_t variable.
 * @param bits bits to wait for
 * @param clear flags EVENT_BITS_CLEAR or EVENT_BITS_LEAVE
 * @return 0 if no any of requested bits are set
 *         list of bits from the input argument, which were set
 */
uint8_t tiny_events_check_int(tiny_events_t *event, uint8_t bits, uint8_t clear);

/**
 * Sets bits for cross-platform event group object.
 * @param event pointer to tiny_event_t variable.
 * @param bits bits to set
 */
void tiny_events_set(tiny_events_t *event, uint8_t bits);

/**
 * Clears bits for cross-platform event group object.
 * @param event pointer to tiny_event_t variable.
 * @param bits bits to clear
 */
void tiny_events_clear(tiny_events_t *event, uint8_t bits);

/**
 * Sleeps for specified period in milliseconds.
 * @param ms time in milliseconds to sleep
 */
void tiny_sleep(uint32_t ms);

/**
 * Returns timestamp in milliseconds since system started up.
 */
uint32_t tiny_millis();

/**
 * Sets logging level if tiny library is compiled with logs
 * @param level log level to set, or 0 to disable logs
 */
void tiny_log_level(uint8_t level);

#ifdef __cplusplus
}
#endif

