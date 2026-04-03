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

#include "tiny_fd.h"
#include "tiny_fd_int.h"
#include "tiny_fd_tx_int.h"
#include "tiny_fd_i_queue_control_int.h"
#include "tiny_fd_defines_int.h"
#include "tiny_fd_peers_int.h"
#include "tiny_fd_service_queue_int.h"
#include <stdint.h>

static void __switch_to_connected_state(tiny_fd_handle_t handle, uint8_t peer);
static void __switch_to_disconnected_state(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

static int __check_received_frame(tiny_fd_handle_t handle, uint8_t peer, uint8_t ns)
{
    int result = TINY_SUCCESS;
    if ( ns == __i_queue_control_get_next_frame_to_receive(&handle->peers[peer].i_queue_control) )
    {
        // This is what we've been waiting for.
        // If new frame number is the one we expect, we update next expected frame number
        // and we clear sent reject flag to 0 in order to allow REJ frame to be sent again.

        // LOG("[%p] Confirming received frame <= %d\n", handle, ns);
        __i_queue_control_move_to_next_frame_to_receive(&handle->peers[peer].i_queue_control);
        handle->peers[peer].sent_reject = 0;
    }
    else
    {
        // The frame we received is not the one we expected.
        // We need to inform remote side about this by sending REJ frame.
        // We want to see next_nr frame
        LOG(TINY_LOG_WRN, "[%p] Out of order I-Frame N(s)=%d\n", handle, ns);
        if ( !handle->peers[peer].sent_reject )
        {
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
                .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_REJ |
                    (__i_queue_control_get_next_frame_to_receive(&handle->peers[peer].i_queue_control) << 5),
            };
            handle->peers[peer].sent_reject = 1;
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, sizeof(tiny_frame_header_t));
        }
        result = TINY_ERR_FAILED;
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

static int __on_i_frame_read(tiny_fd_handle_t handle, uint8_t peer, void *data, int len)
{
    uint8_t control = ((uint8_t *)data)[1];
    uint8_t nr = control >> 5;
    uint8_t ns = (control >> 1) & 0x07;
    LOG(TINY_LOG_INFO, "[%p] Receiving I-Frame N(R-sender awaits)=%02X,N(S-seq received)=%02X with address [%02X]\n", handle, nr, ns, ((uint8_t *)data)[0]);
    int result = __check_received_frame(handle, peer, ns);
    // Confirm all previously sent frames up to received N(R)
    __confirm_sent_frames(handle, peer, nr);
    // Provide data to user only if we expect this frame
    if ( result == TINY_SUCCESS )
    {
        if ( handle->on_read_cb )
        {
            tiny_mutex_unlock(&handle->frames.mutex);
            handle->on_read_cb(handle->user_data,
                               __is_primary_station( handle ) ? (__peer_to_address_field( handle, peer ) >> 2) : TINY_FD_PRIMARY_ADDR,
                               (uint8_t *)data + 2, len - 2);
            tiny_mutex_lock(&handle->frames.mutex);
        }
        // Decide whenever we need to send RR after user callback
        // Check if we need to send confirmations separately. If we have something to send, just skip RR S-frame.
        // Also at this point, since we received expected frame, sent_reject will be cleared to 0.
        if ( __all_frames_are_sent(&handle->peers[peer].i_queue_control) && handle->peers[peer].sent_nr != __i_queue_control_get_next_frame_to_receive(&handle->peers[peer].i_queue_control) )
        {
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ),
                .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RR |
                    (__i_queue_control_get_next_frame_to_receive(&handle->peers[peer].i_queue_control) << 5),
            };
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, 2);
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

static int __on_s_frame_read(tiny_fd_handle_t handle, uint8_t peer, void *data, int len)
{
    uint8_t address = ((uint8_t *)data)[0];
    uint8_t control = ((uint8_t *)data)[1];
    uint8_t nr = control >> 5;
    int result = TINY_ERR_FAILED;
    LOG(TINY_LOG_INFO, "[%p] Receiving S-Frame N(R)=%02X, type=%s with address [%02X]\n", handle, nr,
        ((control >> 2) & 0x03) == 0x00 ? "RR" : ((control >> 2) & 0x03) == 0x01 ? "RNR" : ((control >> 2) & 0x03) == 0x02 ? "REJ" : "SREJ", ((uint8_t *)data)[0]);
    if ( (control & HDLC_S_FRAME_TYPE_MASK) == HDLC_S_FRAME_TYPE_REJ )
    {
        // Confirm all previously sent frames up to received N(R)
        __confirm_sent_frames(handle, peer, nr);
        __resend_all_unconfirmed_frames(handle, peer, control, nr);
    }
    else if ( (control & HDLC_S_FRAME_TYPE_MASK) == HDLC_S_FRAME_TYPE_RR )
    {
        // Confirm all previously sent frames up to received N(R)
        __confirm_sent_frames(handle, peer, nr);
        if ( address & HDLC_CR_BIT )
        {
            // Send answer if we don't have frames to send
            if ( __all_frames_are_sent(&handle->peers[peer].i_queue_control) )
            {
                tiny_frame_header_t frame = {
                    .address = __peer_to_address_field( handle, peer ),
                    .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RR |
                        (control & HDLC_P_BIT) |
                        (__i_queue_control_get_next_frame_to_receive(&handle->peers[peer].i_queue_control) << 5),
                };
                __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, 2);
            }
        }
    }
    else if ( (control & HDLC_S_FRAME_TYPE_MASK) == HDLC_S_FRAME_TYPE_RNR )
    {
        // Confirm all previously sent frames up to received N(R), but peer is busy — do not send more
        __confirm_sent_frames(handle, peer, nr);
        LOG(TINY_LOG_WRN, "[%p] Peer signalled RNR (busy), pausing I-frame transmission\n", handle);
    }
    else if ( (control & HDLC_S_FRAME_TYPE_MASK) == HDLC_S_FRAME_TYPE_SREJ )
    {
        // SREJ: confirm frames up to N(R), retransmit only frame N(R)
        __confirm_sent_frames(handle, peer, nr);
        LOG(TINY_LOG_INFO, "[%p] SREJ received for frame N(R)=%d, scheduling selective retransmit\n", handle, nr);
        handle->peers[peer].srej_req_mask |= (1 << nr);
        tiny_events_set(&handle->events, FD_EVENT_TX_DATA_AVAILABLE);
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

static int __on_u_frame_read(tiny_fd_handle_t handle, uint8_t peer, void *data, int len)
{
    uint8_t control = ((uint8_t *)data)[1];
    uint8_t type = control & HDLC_U_FRAME_TYPE_MASK;
    int result = TINY_ERR_FAILED;
    LOG(TINY_LOG_INFO, "[%p] Receiving U-Frame type=%02X with address [%02X]\n", handle, type, ((uint8_t *)data)[0]);
    // In disconnected state, only accept connection setup (SABM/SNRM) and connectionless (UI) frames.
    // Respond with DM to commands; silently ignore responses.
    if ( handle->peers[peer].state == TINY_FD_STATE_DISCONNECTED &&
         type != HDLC_U_FRAME_TYPE_SABM && type != HDLC_U_FRAME_TYPE_SNRM && type != HDLC_U_FRAME_TYPE_UI )
    {
        LOG(TINY_LOG_WRN, "[%p] Ignoring U-frame type=%02X in disconnected state\n", handle, type);
        if ( ((uint8_t *)data)[0] & HDLC_CR_BIT )
        {
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ),
                .control = HDLC_U_FRAME_TYPE_DM | HDLC_U_FRAME_BITS,
            };
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2);
        }
        return result;
    }
    if ( type == HDLC_U_FRAME_TYPE_SABM || type == HDLC_U_FRAME_TYPE_SNRM )
    {
        tiny_frame_header_t frame = {
            .address = __peer_to_address_field( handle, peer ),
            .control = HDLC_U_FRAME_TYPE_UA | HDLC_U_FRAME_BITS,
        };
        __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2);
        if ( handle->peers[peer].state != TINY_FD_STATE_DISCONNECTED && handle->peers[peer].state != TINY_FD_STATE_CONNECTING )
        {
            __switch_to_disconnected_state(handle, peer);
        }
        __switch_to_connected_state(handle, peer);
    }
    else if ( type == HDLC_U_FRAME_TYPE_DISC )
    {
        tiny_frame_header_t frame = {
            .address = __peer_to_address_field( handle, peer ),
            .control = HDLC_U_FRAME_TYPE_UA | HDLC_U_FRAME_BITS,
        };
        __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2);
        __switch_to_disconnected_state(handle, peer);
    }
    else if ( type == HDLC_U_FRAME_TYPE_RSET )
    {
        // RSET resets sequence numbers and expects UA response
        LOG(TINY_LOG_WRN, "[%p] RSET received, resetting sequence numbers\n", handle);
        __reset_i_queue_control(&handle->peers[peer].i_queue_control);
        handle->peers[peer].sent_nr = 0;
        handle->peers[peer].sent_reject = 0;
        handle->peers[peer].srej_req_mask = 0;
        tiny_frame_header_t frame = {
            .address = __peer_to_address_field( handle, peer ),
            .control = HDLC_U_FRAME_TYPE_UA | HDLC_U_FRAME_BITS,
        };
        __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2);
    }
    else if ( type == HDLC_U_FRAME_TYPE_FRMR )
    {
        // FRMR indicates protocol error on remote side — initiate link reset via SABM/SNRM
        LOG(TINY_LOG_ERR, "[%p] FRMR received, initiating link reset\n", handle);
        if ( handle->peers[peer].state != TINY_FD_STATE_DISCONNECTED )
        {
            __switch_to_disconnected_state(handle, peer);
        }
        tiny_frame_header_t sabm_frame = {
            .address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
            .control = (handle->mode == TINY_FD_MODE_NRM ? HDLC_U_FRAME_TYPE_SNRM : HDLC_U_FRAME_TYPE_SABM) | HDLC_U_FRAME_BITS,
        };
        __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &sabm_frame, 2);
        handle->peers[peer].state = TINY_FD_STATE_CONNECTING;
    }
    else if ( type == HDLC_U_FRAME_TYPE_UA )
    {
        if ( handle->peers[peer].state == TINY_FD_STATE_CONNECTING )
        {
            // confirmation received
            __switch_to_connected_state(handle, peer);
        }
        else if ( handle->peers[peer].state == TINY_FD_STATE_DISCONNECTING )
        {
            __switch_to_disconnected_state(handle, peer);
        }
    }
    else if ( type == HDLC_U_FRAME_TYPE_DM )
    {
        // DM indicates the remote side is in disconnected mode
        LOG(TINY_LOG_WRN, "[%p] DM received, peer is in disconnected mode\n", handle);
        if ( handle->peers[peer].state != TINY_FD_STATE_DISCONNECTED )
        {
            __switch_to_disconnected_state(handle, peer);
        }
    }
    else if ( type == HDLC_U_FRAME_TYPE_UI )
    {
        // UI (Unnumbered Information) — connectionless data, accepted in any state
        LOG(TINY_LOG_INFO, "[%p] UI frame received, len=%d\n", handle, len);
        if ( handle->on_read_ui_cb && len > 2 )
        {
            tiny_mutex_unlock(&handle->frames.mutex);
            handle->on_read_ui_cb(handle->user_data,
                                   __is_primary_station( handle ) ? (__peer_to_address_field( handle, peer ) >> 2) : TINY_FD_PRIMARY_ADDR,
                                   (uint8_t *)data + 2, len - 2);
            tiny_mutex_lock(&handle->frames.mutex);
        }
    }
    else
    {
        LOG(TINY_LOG_WRN, "[%p] Unknown hdlc U-frame received\n", handle);
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////
