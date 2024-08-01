/*
    Copyright 2019-2024 (C) Alexey Dynda

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
#include "tiny_fd_data_queue_int.h"
#include "tiny_fd_defines_int.h"
#include "tiny_fd_peers_int.h"
#include "tiny_fd_service_queue_int.h"
#include <stdint.h>

static void __switch_to_connected_state(tiny_fd_handle_t handle, uint8_t peer);
static void __switch_to_disconnected_state(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

static void __confirm_sent_frames(tiny_fd_handle_t handle, uint8_t peer, uint8_t nr)
{
    // Repeat the loop for all frames that are not confirmed yet till we reach N(r)
    while ( nr != handle->peers[peer].confirm_ns )
    {
        // if we reached the last sent frame index, but we have something to confirm
        // it means that remote side is out of sync.
        if ( handle->peers[peer].confirm_ns == handle->peers[peer].last_ns )
        {
            // TODO: Out of sync
            // No solution for this part yet.
            LOG(TINY_LOG_CRIT, "[%p] Confirmation contains wrong N(r). Remote side is out of sync\n", handle);
            break;
        }
        uint8_t address = __peer_to_address_field( handle, peer );
        // LOG("[%p] Confirming sent frames %d\n", handle, handle->peers[peer].confirm_ns);
        // Call on_send_cb to inform application that frame was sent
        tiny_fd_frame_info_t *slot = tiny_fd_queue_get_next( &handle->frames.i_queue, TINY_FD_QUEUE_I_FRAME, address, handle->peers[peer].confirm_ns );
        if ( slot != NULL )
        {
            if ( handle->on_send_cb )
            {
                tiny_mutex_unlock(&handle->frames.mutex);
                handle->on_send_cb(handle->user_data,
                                   __is_primary_station( handle ) ? (__peer_to_address_field( handle, peer ) >> 2) : TINY_FD_PRIMARY_ADDR,
                                   &slot->payload[0], slot->len);
                tiny_mutex_lock(&handle->frames.mutex);
            }
            tiny_fd_queue_free( &handle->frames.i_queue, slot );
            if ( tiny_fd_queue_has_free_slots( &handle->frames.i_queue ) )
            {
                // Unblock tx queue to allow application to put new frames for sending
                tiny_events_set(&handle->events, FD_EVENT_QUEUE_HAS_FREE_SLOTS);
            }
        }
        else
        {
            // This should never happen !!!
            // TODO: Add error processing
            LOG(TINY_LOG_ERR, "[%p] The frame cannot be confirmed: %02X\n", handle, handle->peers[peer].confirm_ns);
        }
        handle->peers[peer].confirm_ns = (handle->peers[peer].confirm_ns + 1) & seq_bits_mask;
        handle->peers[peer].retries = handle->retries;
    }
    // Check if we can accept new frames from the application.
    if ( __can_accept_i_frames( handle, peer ) )
    {
        // Unblock specific peer to accept new frames for sending
        tiny_events_set(&handle->peers[peer].events, FD_EVENT_CAN_ACCEPT_I_FRAMES);
    }
    LOG(TINY_LOG_DEB, "[%p] Last confirmed frame: %02X\n", handle, handle->peers[peer].confirm_ns);
    // LOG("[%p] N(S)=%d, N(R)=%d\n", handle, handle->peers[peer].confirm_ns, handle->peers[peer].next_nr);
}

///////////////////////////////////////////////////////////////////////////////

static int __check_received_frame(tiny_fd_handle_t handle, uint8_t peer, uint8_t ns)
{
    int result = TINY_SUCCESS;
    if ( ns == handle->peers[peer].next_nr )
    {
        // This is what we've been waiting for.
        // If new frame number is the one we expect, we update next expected frame number
        // and we clear sent reject flag to 0 in order to allow REJ frame to be sent again.

        // LOG("[%p] Confirming received frame <= %d\n", handle, ns);
        handle->peers[peer].next_nr = (handle->peers[peer].next_nr + 1) & seq_bits_mask;
        handle->peers[peer].sent_reject = 0;
    }
    else
    {
        // The frame we received is not the one we expected.
        // We need to inform remote side about this by sending REJ frame.
        // We want to see next_nr frame
        LOG(TINY_LOG_ERR, "[%p] Out of order I-Frame N(s)=%d\n", handle, ns);
        if ( !handle->peers[peer].sent_reject )
        {
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
                .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_REJ | (handle->peers[peer].next_nr << 5),
            };
            handle->peers[peer].sent_reject = 1;
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, sizeof(tiny_frame_header_t));
        }
        result = TINY_ERR_FAILED;
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

static void __resend_all_unconfirmed_frames(tiny_fd_handle_t handle, uint8_t peer, uint8_t control, uint8_t nr)
{
    // First, we need to check if that is possible. Maybe remote side is not in sync
    while ( handle->peers[peer].next_ns != nr )
    {
        if ( handle->peers[peer].confirm_ns == handle->peers[peer].next_ns )
        {
            // consider here that remote side is not in sync, we cannot perform request
            LOG(TINY_LOG_CRIT, "[%p] Remote side not in sync\n", handle);
            tiny_fd_u_frame_t frame = {
                .header.address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
                .header.control = HDLC_U_FRAME_TYPE_FRMR | HDLC_U_FRAME_BITS,
                .data1 = control,
                .data2 = (handle->peers[peer].next_nr << 5) | (handle->peers[peer].next_ns << 1),
            };
            // Send 2-byte header + 2 extra bytes
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 4);
            break;
        }
        handle->peers[peer].next_ns = (handle->peers[peer].next_ns - 1) & seq_bits_mask;
    }
    LOG(TINY_LOG_DEB, "[%p] N(s) is set to %02X\n", handle, handle->peers[peer].next_ns);
    tiny_events_set(&handle->events, FD_EVENT_TX_DATA_AVAILABLE);
}

///////////////////////////////////////////////////////////////////////////////

static int __on_i_frame_read(tiny_fd_handle_t handle, uint8_t peer, void *data, int len)
{
    uint8_t control = ((uint8_t *)data)[1];
    uint8_t nr = control >> 5;
    uint8_t ns = (control >> 1) & 0x07;
    LOG(TINY_LOG_INFO, "[%p] Receiving I-Frame N(R)=%02X,N(S)=%02X with address [%02X]\n", handle, nr, ns, ((uint8_t *)data)[0]);
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
        if ( __all_frames_are_sent(handle, peer) && handle->peers[peer].sent_nr != handle->peers[peer].next_nr )
        {
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ),
                .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RR | (handle->peers[peer].next_nr << 5),
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
        ((control >> 2) & 0x03) == 0x00 ? "RR" : "REJ", ((uint8_t *)data)[0]);
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
            if ( handle->peers[peer].next_ns == handle->peers[peer].last_ns )
            {
                tiny_frame_header_t frame = {
                    .address = __peer_to_address_field( handle, peer ),
                    .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RR | (handle->peers[peer].next_nr << 5),
                };
                __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, 2);
            }
        }
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
    if ( type == HDLC_U_FRAME_TYPE_SABM || type == HDLC_U_FRAME_TYPE_SNRM )
    {
        tiny_frame_header_t frame = {
            .address = __peer_to_address_field( handle, peer ),
            .control = HDLC_U_FRAME_TYPE_UA | HDLC_U_FRAME_BITS,
        };
        __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2);
        if ( handle->peers[peer].state == TINY_FD_STATE_CONNECTED )
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
        // resets N(R) = 0 in secondary, resets N(S) = 0 in primary
        // expected answer UA
    }
    else if ( type == HDLC_U_FRAME_TYPE_FRMR )
    {
        // response of secondary in case of protocol errors: invalid control field, invalid N(R),
        // information field too long or not expected in this frame
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
    else
    {
        LOG(TINY_LOG_WRN, "[%p] Unknown hdlc U-frame received\n", handle);
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////