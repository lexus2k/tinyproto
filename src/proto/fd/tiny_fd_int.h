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

#pragma once

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define TINY_FD_U_QUEUE_MAX_SIZE 4

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "proto/hdlc/tiny_hdlc.h"
#include "hal/tiny_types.h"

#define FD_MIN_BUF_SIZE(mtu,window) (sizeof(tiny_fd_data_t) + \
                 (sizeof(tiny_i_frame_info_t *) + sizeof(tiny_i_frame_info_t) + mtu) * ( window + 1 ))

#define FD_MTU_SIZE(bsize,window) ( (bsize - sizeof(tiny_fd_data_t)) / (window + 1) - \
                 (sizeof(tiny_i_frame_info_t *) + sizeof(tiny_i_frame_info_t)) )

typedef enum
{
    TINY_FD_STATE_DISCONNECTED,
    TINY_FD_STATE_CONNECTING,
    TINY_FD_STATE_CONNECTED_ABM,
} tiny_fd_state_t;

typedef struct
{
    uint8_t address;
    uint8_t control;
} tiny_frame_header_t;

typedef struct
{
    tiny_frame_header_t header; ///< header of s-frame, non-empty is there is something to send
} tiny_s_frame_info_t;

typedef struct
{
    tiny_frame_header_t header; ///< header of u-frame, non-empty is there is something to send
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;
} tiny_u_frame_info_t;

typedef struct
{
    uint8_t type; ///< 0 for now
    int len;
    tiny_frame_header_t header; ///< header, fill every time, when user payload is sending
    uint8_t user_payload; ///< this byte and all bytes after are user payload
} tiny_i_frame_info_t;

typedef struct
{
    int len;
    union
    {
        tiny_s_frame_info_t s_frame;
        tiny_u_frame_info_t u_frame;
    };
} tiny_frame_info_t;

typedef struct
{
    tiny_i_frame_info_t **i_frames;
    uint8_t max_i_frames;
    uint8_t head_ptr;

    uint8_t *rx_buffer;
    uint8_t *tx_buffer;
    int mtu;

    tiny_mutex_t mutex;
    uint8_t next_nr; // frame waiting to receive
    uint8_t sent_nr; // frame index last sent back
    uint8_t sent_reject; // If reject was already sent
    uint8_t next_ns; // next frame to be sent
    uint8_t confirm_ns; // next frame to be confirmed
    uint8_t last_ns; // next free frame in cycle buffer

    uint32_t last_i_ts; // last sent I-frame timestamp
    uint32_t last_ka_ts; // last keep alive timestamp
    uint8_t  ka_confirmed;

    uint8_t retries; // Number of retries to perform before timeout takes place

    tiny_events_t events;
} tiny_frames_info_t;

typedef struct tiny_fd_data_t
{
    /// hdlc information
    hdlc_struct_t     _hdlc;
    /// state of hdlc protocol according to ISO & RFC
    tiny_fd_state_t    state;
    /// callback function to write bytes to the physical channel
    write_block_cb_t   write_func;
    /// callback function to read bytes from the physical channel
    read_block_cb_t    read_func;
    /// Callback to process received frames
    on_frame_cb_t      on_frame_cb;
    /// Callback to get notification of sent frames
    on_frame_cb_t      on_sent_cb;
    /// Timeout for operations with acknowledge
    uint16_t           send_timeout;
    /// Timeout before retrying resend I-frames
    uint16_t           retry_timeout;
    /// Timeout before sending keep alive HDLC frame (RR)
    uint16_t           ka_timeout;
    /// Number of retries to perform before timeout takes place
    uint8_t            retries;
    /// Information for frames being processed
    tiny_frames_info_t frames;
    struct
    {
        tiny_frame_info_t queue[TINY_FD_U_QUEUE_MAX_SIZE];
        uint8_t queue_ptr;
        uint8_t queue_len;
    } s_u_frames;
    /// user specific data
    void *             user_data;
    // pointer to user context 
    void *             user_context;
} tiny_fd_data_t;

#ifdef __cplusplus
}
#endif

#endif
