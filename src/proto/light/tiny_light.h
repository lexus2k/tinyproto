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
 This is Tiny Light protocol implementation for microcontrollers

 @file
 @brief Tiny Light protocol API
*/
#ifndef _TINY_LIGHT_H_
#define _TINY_LIGHT_H_

#include "proto/hdlc/low_level/hdlc.h"
#include "hal/tiny_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @defgroup LIGHT_API Tiny light protocol API functions
 * @{
 *
 * @brief Simple SLIP-like framing protocol with optional CRC.
 *
 * @details The Light protocol provides the simplest way to send and receive
 *          framed messages. It uses HDLC low-level framing under the hood but
 *          adds no acknowledgments or retransmission. It has very low memory
 *          consumption (~800 bytes of flash).
 *
 *          Use this protocol when:
 *          - You need minimal overhead and low flash usage
 *          - The transport is reliable enough that lost frames are acceptable
 *          - You want blocking read/write semantics
 *
 *          For reliable delivery with automatic retransmission, use the
 *          Full-Duplex protocol (\ref FULL_DUPLEX_API).
 */

/*************************************************************
 *
 *          Tiny defines
 *
 *************************************************************/

/**
 * This macro defines buffer size required for tiny light protocol
 */
#define LIGHT_BUF_SIZE (sizeof(uintptr_t) * 18)

    /**
     * This structure contains information about communication channel and its state.
     * \warning This is for internal use only, and should not be accessed directly from the application.
     */
    typedef struct
    {
        /// pointer to platform related write function
        write_block_cb_t write_func;
        /// pointer to platform related read function
        read_block_cb_t read_func;
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        hdlc_ll_handle_t _hdlc;
        int rx_len;
        TINY_ALIGNED_STRUCT uint8_t buffer[LIGHT_BUF_SIZE];
#endif
        /// user-specific data
        void *user_data;
        /// CRC type to use
        hdlc_crc_t crc_type;
    } STinyLightData;

    /**
     * The function initializes internal structures for Tiny channel and return handle
     * to be used with all Tiny and IPC functions.
     * @param handle - pointer to Tiny Light data
     * @param write_func - pointer to write data function (to communication channel).
     * @param read_func - pointer to read function (from communication channel).
     *        read_func is not required and should be NULL, if event-based API of Tiny Protocol is used.
     * @param pdata - pointer to a user private data. This pointer is passed to write_func/read_func.
     * @see write_block_cb_t
     * @see read_block_cb_t
     * @return TINY_SUCCESS or error code.
     * @remarks This function is not thread safe.
     */
    extern int tiny_light_init(STinyLightData *handle, write_block_cb_t write_func, read_block_cb_t read_func,
                               void *pdata);

    /**
     * The function closes  channel.
     * @param handle - pointer to Tiny Light data.
     * @see tiny_init()
     * @return TINY_ERR_INVALID_DATA, TINY_SUCCESS.
     * @remarks This function is not thread safe.
     */
    extern int tiny_light_close(STinyLightData *handle);

    /**
     * @brief sends frame with user payload to communication channel in blocking mode
     *
     * The function sends data to communication channel.
     * The function works in blocking mode, i.e. it returns
     * control only if user data are successfully sent, or in case of error.
     * @param handle - pointer to Tiny Light data.
     * @param pbuf - a const pointer to unsigned char - buffer with data to send
     * @param len - an integer argument - length of data to send
     * @see TINY_ERR_INVALID_DATA
     * @see TINY_ERR_FAILED
     * @return TINY_ERR_INVALID_DATA, TINY_ERR_FAILED or number of sent bytes.
     * @remarks This function is thread safe.
     */
    extern int tiny_light_send(STinyLightData *handle, const uint8_t *pbuf, int len);

    /**
     * @brief reads frame from the channel in blocking mode.
     *
     * The function reads user data from communication channel
     * @param handle - pointer to Tiny Light data.
     * @param pbuf a const pointer to unsigned char - buffer with data to send
     * @param len an integer argument - length of data to send
     * @see TINY_ERR_INVALID_DATA
     * @see TINY_ERR_FAILED
     * @see TINY_ERR_DATA_TOO_LARGE
     * @see TINY_ERR_OUT_OF_SYNC
     * @see TINY_ERR_BUSY
     * @return TINY_ERR_INVALID_DATA, TINY_ERR_FAILED, TINY_ERR_OUT_OF_SYNC, TINY_ERR_BUSY, TINY_ERR_DATA_TOO_LARGE
     *         or number of sent bytes.
     * @note TINY_ERR_DATA_TOO_LARGE can be returned in successful case. If frame is received, but passed buffer
     *       to the function is too small to fit all.
     * @remarks This function is not thread safe.
     */
    extern int tiny_light_read(STinyLightData *handle, uint8_t *pbuf, int len);

    /**
     * @brief returns lower level hdlc handle.
     *
     * Returns lower level hdlc handle to use with low level function.
     * If you use high level light API, please be careful with low-level hdlc functions in
     * case you mix the calls.
     *
     * @param handle - pointer to Tiny Light data.
     * @return hdlc handle or NULL
     */
    extern hdlc_ll_handle_t tiny_light_get_hdlc(STinyLightData *handle);

    /**
     * @}
     */

#ifdef __cplusplus
}
#endif

#endif /* _TINY_PROTOCOL_H_ */
