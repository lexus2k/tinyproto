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
#pragma once

#include "hal/tiny_types.h"
#include "proto/crc/tiny_crc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Byte to fill gap between frames */
#define TINY_HDLC_FILL_BYTE 0xFF

    /**
     * @defgroup HDLC_LOW_LEVEL_API HDLC low level protocol API
     * @{
     *
     * @brief Low-level HDLC framing: flag bytes, byte stuffing, and CRC.
     *
     * @details This module implements low-level HDLC framing according to
     *          RFC 1662 and ISO 3309-1979. It handles:
     *          - Frame delimiters (0x7E)
     *          - Byte stuffing / transparency (0x7D escape sequences)
     *          - CRC calculation and validation (8-bit, FCS-16, FCS-32, or none)
     *
     *          This is a stateless framing layer — it does not implement acknowledgments,
     *          sequence numbers, or retransmission. For reliable delivery, use the
     *          Full-Duplex protocol (\ref FULL_DUPLEX_API).
     *
     *          The hdlc_ll functions do not use any synchronization primitives (mutexes, etc.),
     *          making them completely platform-independent and safe for use in interrupt context
     *          (with care).
     */

    /**
     * Flags for hdlc_ll_reset function
     */
    typedef enum
    {
        HDLC_LL_RESET_BOTH = 0x00,
        HDLC_LL_RESET_TX_ONLY = 0x01,
        HDLC_LL_RESET_RX_ONLY = 0x02,
    } hdlc_ll_reset_flags_t;

    struct hdlc_ll_data_t;

    /** Handle for HDLC low level protocol */
    typedef struct hdlc_ll_data_t *hdlc_ll_handle_t;

    /**
     * Structure describes configuration of lowest HDLC level
     * Initialize this structure by 0 before passing to hdlc_ll_init()
     * function.
     */
    typedef struct
    {
        /**
         * User-defined callback, which is called when new packet arrives from hw
         * channel. The context of this callback is context, where hdlc_ll_run_rx() is
         * called from.
         * @param user_data user-defined data
         * @param data pointer to received data
         * @param len size of received data in bytes
         */

        on_frame_cb_t on_frame_read;

        /**
         * User-defined callback, which is called when the packet is sent to TX
         * channel. The context of this callback is context, where hdlc_ll_run_tx() is
         * called from.
         * @param user_data user-defined data
         * @param data pointer to sent data
         * @param len size of sent data in bytes
         */
        on_tx_frame_cb_t on_frame_send;

        /**
         * Buffer to be used by hdlc level to receive data to.
         * Use hdlc_ll_get_buf_size() to calculate minimum buffer size.
         */
        void *buf;

        /**
         * size of hdlc buffer
         */
        int buf_size;

        /**
         * crc field type to use on hdlc level.
         * If HDLC_CRC_DEFAULT is passed, crc type will be selected automatically (depending on library configuration),
         * but HDLC_CRC_16 has higher priority.
         */
        hdlc_crc_t crc_type;

        /** User data, which will be passed to user-defined callback as first argument */
        void *user_data;

        /** mtu size, can be 0 */
        int mtu;
    } hdlc_ll_init_t;

    //------------------------ GENERIC FUNCTIONS ------------------------------

    /**
     * Initializes hdlc level and returns hdlc handle or NULL in case of error.
     *
     * @param handle pointer to hdlc handle variable
     * @param init pointer to hdlc_ll_struct_t structure, which defines user-specific configuration
     * @return -1 if error
     *          0 if success
     */
    int hdlc_ll_init(hdlc_ll_handle_t *handle, hdlc_ll_init_t *init);

    /**
     * Shutdowns all hdlc activity
     *
     * @param handle hdlc handle
     */
    int hdlc_ll_close(hdlc_ll_handle_t handle);

    /**
     * Resets hdlc state. Use this function, if hw error happened on tx or rx
     * line, and this requires hardware change, and cancelling current operation.
     *
     * @param handle hdlc handle
     * @param flags HDLC_LL_RESET_TX_ONLY, HDLC_LL_RESET_RX_ONLY, HDLC_LL_RESET_BOTH
     */
    void hdlc_ll_reset(hdlc_ll_handle_t handle, uint8_t flags);

    //------------------------ RX FUNCTIONS ------------------------------

    /**
     * Processes incoming data. Implementation of reading data from hw is user
     * responsibility. If hdlc_ll_run_rx() returns value less than size of data
     * passed to the function, then hdlc_ll_run_rx() must be called later second
     * time with the pointer to and size of not processed bytes.
     *
     * If you don't care about errors on RX line, it is allowed to ignore
     * all error codes except TINY_ERR_FAILED, which means general failure.
     *
     * if hdlc_ll_run_rx() returns 0 bytes processed, just call it once again.
     * It is guaranteed, that at least second call will process bytes.
     *
     * @note this API can be used in interrupt context, but please implement
     *       on_frame_read callback carefully.
     *
     * This function will return the following codes in error field:
     *   - TINY_ERR_DATA_TOO_LARGE if receiving data fails to fit incoming buffer
     *   - TINY_ERR_FAILED if generic failure happened
     *   - TINY_ERR_WRONG_CRC if crc field of incoming frame is incorrect
     *   - TINY_SUCCESS if operation completed successfully
     *
     * @param handle hdlc handle
     * @param data pointer to incoming data to process
     * @param len size of received data in bytes
     * @param error pointer to store error code. If no error, 0 is returned.
     *        this argument can be NULL.
     * @return number of processed bytes from specified data buffer.
     *         Never returns negative value
     */
    int hdlc_ll_run_rx(hdlc_ll_handle_t handle, const void *data, int len, int *error);

    //------------------------ TX FUNCTIONS ------------------------------

    /**
     * If hdlc protocol has some data to send it will full data with
     * This function returns either if no more data to send, or specified
     * buffer is filled completely.
     *
     * @param handle hdlc handle
     * @param data pointer to buffer to fill with data
     * @param len length of specified buffer
     * @return number of bytes written to specified buffer
     */
    int hdlc_ll_run_tx(hdlc_ll_handle_t handle, void *data, int len);

    /**
     * Puts next frame for sending.
     *
     * Deprecated: use hdlc_ll_put_frame() instead.
     * hdlc_ll_put() function will not wait or perform send operation, but only pass data pointer to
     * hdlc state machine. In this case, some other thread needs to
     * or in the same thread you need to send data using hdlc_ll_get_tx_data().
     *
     * @param handle hdlc handle
     * @param data pointer to new data to send
     * @param len size of data to send in bytes
     * @return TINY_ERR_BUSY if TX queue is busy with another frame.
     *         TINY_ERR_INVALID_DATA if len is zero.
     *         TINY_SUCCESS if data is successfully sent
     * @warning buffer with data must be available all the time until
     *          data are actually sent to tx hw channel. That is 
     *          you need to use callback to understand, when buffer
     *          is not longer needed at hdlc low level. Or you can use
     *          hdlc_ll_reset() to reset hdlc tx state and to stop using
     *          passed buffer.
     * @note TINY_ERR_BUSY and TINY_ERR_INVALID_DATA refer to putting new frame to TX
     *       hdlc queue.
     */
    int hdlc_ll_put(hdlc_ll_handle_t handle, const void *data, int len) __attribute__((deprecated));

    /**
     * Puts next frame for sending.
     *
     * hdlc_ll_put_frame() function will not wait or perform send operation, but only pass data pointer to
     * hdlc state machine. In this case, some other thread needs to
     * or in the same thread you need to send data using hdlc_ll_run_tx().
     *
     * @param handle hdlc handle
     * @param data pointer to new data to send
     * @param len size of data to send in bytes
     * @return TINY_ERR_BUSY if TX queue is busy with another frame.
     *         TINY_ERR_INVALID_DATA if len is zero.
     *         TINY_SUCCESS if data is successfully sent
     * @warning buffer with data must be available all the time until
     *          data are actually sent to tx hw channel. That is 
     *          you need to use callback to understand, when buffer
     *          is not longer needed at hdlc low level. Or you can use
     *          hdlc_ll_reset() to reset hdlc tx state and to stop using
     *          passed buffer.
     * @note TINY_ERR_BUSY and TINY_ERR_INVALID_DATA refer to putting new frame to TX
     *       hdlc queue.
     */
    int hdlc_ll_put_frame(hdlc_ll_handle_t handle, const void *data, int len);

    /**
     * Returns minimum buffer size, required to hold hdlc low level data for desired payload size.
     *
     * @note This function calculates required buffer size based on CRC32
     *
     * @param mtu size of desired max payload in bytes
     * @return size of the buffer required
     */
    int hdlc_ll_get_buf_size(int mtu);

    /**
     * Returns minimum buffer size, required to hold hdlc low level data for desired payload size.
     *
     * @param mtu size of desired max payload in bytes
     * @param crc_type type of crc validation to use for the protocol
     * @param rx_window number of RX frames in the RX ring buffer
     *
     * @return size of the buffer required
     */
    int hdlc_ll_get_buf_size_ex(int mtu, hdlc_crc_t crc_type, int rx_window);

    /**
     * @}
     */

#ifdef __cplusplus
}
#endif
