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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define TINY_FD_U_QUEUE_MAX_SIZE 4

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "proto/hdlc/low_level/hdlc.h"
#include "proto/hdlc/low_level/hdlc_int.h"
#include "hal/tiny_types.h"
#include "tiny_fd_i_queue_control_int.h"
#include "tiny_fd_frames_queue_int.h"

#define FD_PEER_BUF_SIZE() ( sizeof(tiny_fd_peer_info_t) )

#define FD_MIN_BUF_SIZE(mtu, window)                                                                                   \
    (sizeof(tiny_fd_data_t) + TINY_ALIGN_STRUCT_VALUE - 1 + \
     HDLC_MIN_BUF_SIZE(mtu + sizeof(tiny_frame_header_t), HDLC_CRC_16) +                     \
      ( 1 * FD_PEER_BUF_SIZE() ) + \
      (sizeof(tiny_fd_frame_info_t *) + sizeof(tiny_fd_frame_info_t) + mtu \
                                      - sizeof(((tiny_fd_frame_info_t *)0)->payload) ) * window + \
          ( sizeof(tiny_fd_frame_info_t) + sizeof(tiny_fd_frame_info_t *) ) * TINY_FD_U_QUEUE_MAX_SIZE )

#define FD_BUF_SIZE_EX(mtu, tx_window, crc, rx_window)                                                                      \
    (sizeof(tiny_fd_data_t) + TINY_ALIGN_STRUCT_VALUE - 1 + \
     HDLC_BUF_SIZE_EX(mtu + sizeof(tiny_frame_header_t), crc, rx_window) +           \
      ( 1 * FD_PEER_BUF_SIZE() ) + \
      (sizeof(tiny_fd_frame_info_t *) + sizeof(tiny_i_frame_info_t) + mtu \
                                      - sizeof(((tiny_fd_frame_info_t *)0)->payload)) * tx_window + \
       ( sizeof(tiny_fd_frame_info_t) + sizeof(tiny_fd_frame_info_t *) ) * TINY_FD_U_QUEUE_MAX_SIZE)

    typedef enum
    {
        TINY_FD_STATE_IDLE = 0,
        TINY_FD_STATE_DISCONNECTED,
        TINY_FD_STATE_CONNECTING,
        TINY_FD_STATE_CONNECTED,
        TINY_FD_STATE_DISCONNECTING,
    } tiny_fd_state_t;

    typedef struct
    {
        tiny_frame_header_t header;
        uint8_t data1;
        uint8_t data2;
    } tiny_fd_u_frame_t;

    typedef struct
    {
        /// state of hdlc protocol according to ISO & RFC
        tiny_fd_state_t state;
        uint8_t addr;        // Peer address

        uint8_t next_nr;     // frame waiting to receive
        uint8_t sent_nr;     // frame index last sent back
        uint8_t sent_reject; // If reject was already sent
        uint8_t next_ns;     // next frame to be sent
        uint8_t confirm_ns;  // next frame to be confirmed

        i_queue_control_t i_queue_control;

        uint32_t last_sent_i_ts;           // last sent I-frame timestamp
        uint32_t last_sent_frame_ts;       // last sent keep alive timestamp
        uint32_t last_received_frame_ts;   // last keep alive timestamp
        uint8_t ka_confirmed;
        uint8_t retries;     // Number of retries to perform before timeout takes place

        tiny_events_t events;

    } tiny_fd_peer_info_t;

    typedef struct tiny_fd_data_t
    {
        /// Callback to process received frames
        on_frame_read_cb_t on_read_cb;
        /// Callback to process received frames
        on_frame_send_cb_t on_send_cb;
        /// Callback to get connect/disconnect notification
        on_connect_event_cb_t on_connect_event_cb;
        /// Callback to log frames
        tiny_fd_log_frame_cb_t log_frame_cb;
        /// hdlc information
        hdlc_ll_handle_t _hdlc;
        /// Timeout for operations with acknowledge
        uint16_t send_timeout;
        /// Timeout before retrying resend I-frames
        uint16_t retry_timeout;
        /// Timeout before sending keep alive HDLC frame (RR)
        uint16_t ka_timeout;
        /// Number of retries to perform before timeout takes place
        uint8_t retries;
        /// Information for frames being processed
        tiny_frames_queue_t frames;
        /// Peers count supported by the primary device
        uint8_t peers_count;
        /// Information on all peers stations
        tiny_fd_peer_info_t *peers;
        /// Local address: 0x00 or for primary devices
        uint8_t addr;
        /// Next peer to process
        uint8_t next_peer;
        /// Last marker timestamp
        uint32_t last_marker_ts;
        /// HDLC mode;
        uint8_t mode;
        /// Global events for HDLC protocol
        tiny_events_t events;
        /// user specific data
        void *user_data;
    } tiny_fd_data_t;

    extern void __tiny_fd_log_frame(
                        tiny_fd_handle_t handle,
                       tiny_fd_frame_direction_t direction,
                       const uint8_t *data,
                       int len);

#ifdef __cplusplus
}
#endif

#endif
