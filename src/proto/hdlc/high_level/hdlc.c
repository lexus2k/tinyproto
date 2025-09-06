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

#include "proto/hdlc/high_level/hdlc.h"
#include "proto/crc/tiny_crc.h"
#include "proto/hdlc/low_level/hdlc_int.h"
#include "hal/tiny_debug.h"

#include <stddef.h>

#ifndef TINY_HDLC_DEBUG
#define TINY_HDLC_DEBUG 0
#endif

#if TINY_HDLC_DEBUG
#define LOG(lvl, fmt, ...) TINY_LOG(lvl, fmt, __VA_ARGS__)
#else
#define LOG(...)
#endif

#define FLAG_SEQUENCE 0x7E
#define FILL_BYTE 0xFF
#define TINY_ESCAPE_CHAR 0x7D
#define TINY_ESCAPE_BIT 0x20

enum
{
    TX_ACCEPT_BIT = 0x01,
    TX_DATA_READY_BIT = 0x02,
    TX_DATA_SENT_BIT = 0x04,
    RX_DATA_READY_BIT = 0x08,
};

static void on_frame_read(void *user_data, uint8_t *data, int len);
static void on_frame_send(void *user_data, const uint8_t *data, int len);

////////////////////////////////////////////////////////////////////////////////////////

hdlc_handle_t hdlc_init(hdlc_struct_t *hdlc_info)
{
    hdlc_ll_init_t init = { 0 };
    init.crc_type = hdlc_info->crc_type;
    init.on_frame_read = on_frame_read;
    init.on_frame_send = on_frame_send;
    init.buf = hdlc_info->rx_buf;
    init.buf_size = hdlc_info->rx_buf_size;
    init.user_data = hdlc_info;
    int err = hdlc_ll_init(&hdlc_info->handle, &init);
    if ( err != TINY_SUCCESS )
    {
        return NULL;
    }

    tiny_events_create(&hdlc_info->events);
    tiny_events_set(&hdlc_info->events, TX_ACCEPT_BIT);
    // Must be last
    hdlc_reset(hdlc_info);
    return hdlc_info;
}

////////////////////////////////////////////////////////////////////////////////////////

int hdlc_close(hdlc_handle_t handle)
{
    if (!handle) // fix: guard against null handle
    {
        return TINY_ERR_INVALID_DATA;
    }
    hdlc_ll_close(handle->handle);
    tiny_events_destroy(&handle->events);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////

void hdlc_reset(hdlc_handle_t handle)
{
    if (!handle) // fix: guard against null handle
        return;
    hdlc_ll_reset(handle->handle, HDLC_LL_RESET_BOTH);
    tiny_events_clear(&handle->events, EVENT_BITS_ALL);
    tiny_events_set(&handle->events, TX_ACCEPT_BIT);
}

////////////////////////////////////////////////////////////////////////////////////////

static void on_frame_send(void *user_data, const uint8_t *data, int len)
{
    hdlc_handle_t handle = (hdlc_handle_t)user_data;
    if ( handle && handle->on_frame_send )
    {
        handle->on_frame_send(handle->user_data, data, len);
    }
    if (handle)
    {
        tiny_events_set(&handle->events, TX_DATA_SENT_BIT);
        tiny_events_set(&handle->events, TX_ACCEPT_BIT);
    }
}

static void hdlc_send_terminate(hdlc_handle_t handle)
{
    LOG(TINY_LOG_INFO, "[HDLC:%p] hdlc_send_terminate HDLC send failed on timeout\n", handle);
    tiny_events_clear(&handle->events, TX_DATA_READY_BIT);
    hdlc_ll_reset(handle->handle, HDLC_LL_RESET_TX_ONLY);
    tiny_events_set(&handle->events, TX_ACCEPT_BIT);
}

////////////////////////////////////////////////////////////////////////////////////////

int hdlc_run_tx(hdlc_handle_t handle)
{
    LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_run_tx ENTER\n", handle);
    int result = 0;
    for ( ;; )
    {
        uint8_t buf[1];
        int temp_result = hdlc_ll_run_tx(handle->handle, buf, sizeof(buf));
        while ( temp_result > 0 )
        {
            int temp = handle->send_tx(handle->user_data, buf, temp_result);
            if ( temp < 0 )
            {
                temp_result = temp;
                break;
            }
            temp_result -= temp;
            result += temp;
        }
        if ( temp_result <= 0 )
        {
#if TINY_HDLC_DEBUG
            if ( temp_result < 0 )
                LOG(TINY_LOG_ERR, "[HDLC:%p] failed to run state with result: %d\n", handle, temp_result);
#endif
            result = result ? result : temp_result;
            break;
        }
    }
    LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_run_tx EXIT\n", handle);
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////

int hdlc_get_tx_data(hdlc_handle_t handle, void *data, int len)
{
    if (!handle || !data || len < 0) // fix: guards
        return TINY_ERR_INVALID_DATA;
    return hdlc_ll_run_tx(handle->handle, data, len);
}

////////////////////////////////////////////////////////////////////////////////////////////

static int hdlc_run_tx_until_sent(hdlc_handle_t handle, uint32_t timeout)
{
    if (!handle) // fix: guard
        return TINY_ERR_INVALID_DATA;
    if (!handle->send_tx) // fix: guard
        return TINY_ERR_INVALID_DATA;

    LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_run_tx_until_sent\n", handle);
    uint32_t ts = tiny_millis();
    int result = 0;
    for (;;)
    {
        uint8_t buf[1];
        result = hdlc_ll_run_tx(handle->handle, buf, sizeof(buf));
        while (result > 0)
        {
            int temp = handle->send_tx(handle->user_data, buf, result);
            if (temp < 0)
            {
                result = temp;
                break;
            }
            result -= temp;
        }

        if (result < 0)
        {
            hdlc_send_terminate(handle);
            LOG(TINY_LOG_ERR, "[HDLC:%p] hdlc_run_tx_until_sent failed: %d\n", handle, result);
            break;
        }
        uint8_t bits = tiny_events_wait(&handle->events, TX_DATA_SENT_BIT, EVENT_BITS_CLEAR, 0);
        if (bits != 0)
        {
            result = TINY_SUCCESS;
            break;
        }
        if ((uint32_t)(tiny_millis() - ts) >= timeout && timeout != 0xFFFFFFFF)
        {
            hdlc_send_terminate(handle);
            result = TINY_ERR_TIMEOUT;
            break;
        }
    };
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int hdlc_put(hdlc_handle_t handle, const void *data, int len, uint32_t timeout)
{
    if (!handle) // fix: guard
        return TINY_ERR_INVALID_DATA;
    // fix: Check for negative len or null data if len > 0
    if (len <= 0 || (!data && len != 0))
    {
        LOG(TINY_LOG_ERR, "[HDLC:%p] invalid data pointer or len\n", handle);
        return TINY_ERR_INVALID_DATA;
    }

    if (tiny_events_wait(&handle->events, TX_ACCEPT_BIT, EVENT_BITS_CLEAR, timeout) == 0)
    {
        LOG(TINY_LOG_WRN, "[HDLC:%p] hdlc_put FAILED\n", handle);
        return TINY_ERR_TIMEOUT;
    }
    hdlc_ll_put_frame(handle->handle, data, len);
    LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_put SUCCESS\n", handle);
    // Indicate that now we have something to send
    tiny_events_set(&handle->events, TX_DATA_READY_BIT);
    return TINY_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////

int hdlc_send(hdlc_handle_t handle, const void *data, int len, uint32_t timeout)
{
    if (!handle) // fix: guard
        return TINY_ERR_INVALID_DATA;

    LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_send (timeout = %u)\n", handle, timeout);
    int result = TINY_SUCCESS;
    // fix: Guard negative len
    if (len < 0)
    {
        LOG(TINY_LOG_ERR, "[HDLC:%p] negative length: %d\n", handle, len);
        return TINY_ERR_INVALID_DATA;
    }

    if (data != NULL)
    {
        result = hdlc_put(handle, data, len, timeout);
        if (result == TINY_ERR_TIMEOUT)
            result = TINY_ERR_BUSY;
    }

    if (result >= 0 && timeout)
    {
        if (handle->multithread_mode)
        {
            LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_send waits for send operation completes (timeout = %u)\n",
                handle, timeout);
            uint8_t bits = tiny_events_wait(&handle->events, TX_DATA_SENT_BIT, EVENT_BITS_CLEAR, timeout);
            result = bits == 0 ? TINY_ERR_TIMEOUT : TINY_SUCCESS;
        }
        else
        {
            result = hdlc_run_tx_until_sent(handle, timeout);
        }
    }
    else if (!timeout)
    {
        LOG(TINY_LOG_DEB, "[HDLC:%p] hdlc_send timeout is zero, exiting\n", handle);
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////

static void on_frame_read(void *user_data, uint8_t *data, int len)
{
    hdlc_handle_t handle = (hdlc_handle_t)user_data;
    if (handle && handle->on_frame_read)
    {
        handle->on_frame_read(handle->user_data, data, len);
    }
    if (handle)
    {
        handle->rx_len = len;
        tiny_events_set(&handle->events, RX_DATA_READY_BIT);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////

int hdlc_run_rx(hdlc_handle_t handle, const void *data, int len, int *error)
{
    if (!handle || !data)
        return TINY_ERR_INVALID_DATA;
    if (len < 0) // fix: negative length check
    {
        if (error) {
            *error = TINY_ERR_INVALID_DATA;
        }
        return 0;
    }
    return hdlc_ll_run_rx(handle->handle, data, len, error);
}
