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

#include "tiny_fd_tx_int.h"
#include "tiny_fd.h"
#include "tiny_fd_int.h"
#include "hal/tiny_debug.h"
#include "tiny_fd_i_queue_control_int.h"
#include "tiny_fd_defines_int.h"
#include "tiny_fd_peers_int.h"
#include "tiny_fd_service_queue_int.h"
#include <stdint.h>


///////////////////////////////////////////////////////////////////////////////

void __confirm_sent_frames(tiny_fd_handle_t handle, uint8_t peer, uint8_t nr)
{
    // Repeat the loop for all frames that are not confirmed yet till we reach N(r)
    while ( nr != handle->peers[peer].confirm_ns )
    {
        // if we reached the last sent frame index, but we have something to confirm
        // it means that remote side is out of sync.
        if ( !__has_unconfirmed_frames(handle, peer) )
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

void __resend_all_unconfirmed_frames(tiny_fd_handle_t handle, uint8_t peer, uint8_t control, uint8_t nr)
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

