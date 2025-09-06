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

#include "tiny_fd_i_queue_control_int.h"

#include "tiny_fd.h"
#include "tiny_fd_int.h"
#include "tiny_fd_defines_int.h"
#include "tiny_fd_peers_int.h"

///////////////////////////////////////////////////////////////////////////////

uint8_t __get_next_frame_to_confirm(tiny_fd_handle_t handle, uint8_t peer)
{
    return handle->peers[peer].i_queue_control.confirm_ns;
}

///////////////////////////////////////////////////////////////////////////////

void __confirm_one_frame(tiny_fd_handle_t handle, uint8_t peer)
{
    handle->peers[peer].i_queue_control.confirm_ns = (handle->peers[peer].i_queue_control.confirm_ns + 1) & seq_bits_mask;
}


///////////////////////////////////////////////////////////////////////////////

bool __has_unconfirmed_frames(tiny_fd_handle_t handle, uint8_t peer)
{
    return (handle->peers[peer].i_queue_control.confirm_ns != handle->peers[peer].i_queue_control.last_ns);
}

///////////////////////////////////////////////////////////////////////////////

bool __all_frames_are_sent(tiny_fd_handle_t handle, uint8_t peer)
{
    return (handle->peers[peer].i_queue_control.last_ns == handle->peers[peer].next_ns);
}

///////////////////////////////////////////////////////////////////////////////

uint32_t __time_passed_since_last_sent_i_frame(tiny_fd_handle_t handle, uint8_t peer)
{
    return (uint32_t)(tiny_millis() - handle->peers[peer].last_sent_i_ts);
}

///////////////////////////////////////////////////////////////////////////////

bool __can_accept_i_frames(tiny_fd_handle_t handle, uint8_t peer)
{
    uint8_t next_last_ns = (handle->peers[peer].i_queue_control.last_ns + 1) & seq_bits_mask;
    bool can_accept = next_last_ns != handle->peers[peer].i_queue_control.confirm_ns;
    return can_accept;
}

///////////////////////////////////////////////////////////////////////////////

bool __put_i_frame_to_tx_queue(tiny_fd_handle_t handle, uint8_t peer, const void *data, int len)
{
    tiny_fd_frame_info_t *slot = tiny_fd_queue_allocate( &handle->frames.i_queue, TINY_FD_QUEUE_I_FRAME, (const uint8_t *)data, len );
    // Check if space is actually available
    if ( slot != NULL )
    {
        LOG(TINY_LOG_DEB, "[%p] QUEUE I-PUT: [%02X] [%02X]\n", handle, slot->header.address, slot->header.control);
        slot->header.address = __peer_to_address_field( handle, peer );
        slot->header.control = handle->peers[peer].i_queue_control.last_ns << 1;
        handle->peers[peer].i_queue_control.last_ns = (handle->peers[peer].i_queue_control.last_ns + 1) & seq_bits_mask;
        tiny_events_set(&handle->events, FD_EVENT_TX_DATA_AVAILABLE);
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////

void __reset_i_queue_control(tiny_fd_handle_t handle, uint8_t peer)
{
    handle->peers[peer].i_queue_control.confirm_ns = 0;
    handle->peers[peer].i_queue_control.last_ns = 0;
    handle->peers[peer].next_ns = 0;
    tiny_fd_queue_reset_for( &handle->frames.i_queue, __peer_to_address_field( handle, peer ) );
}

///////////////////////////////////////////////////////////////////////////////

void __log_i_queue_control_statistics(tiny_fd_handle_t handle, uint8_t peer, uint8_t is_full)
{
    if (is_full)
    {
        LOG(TINY_LOG_WRN, "[%p] I_QUEUE is full N(S-free)queue=%d, N(S-awaiting confirm)confirm=%d, N(S-to send)next=%d\n", handle,
        handle->peers[peer].i_queue_control.last_ns, handle->peers[peer].confirm_ns, handle->peers[peer].next_ns);
    }
    else
    {
    LOG(TINY_LOG_INFO, "[%p] I_QUEUE is N(S)queue=%d, N(S)confirm=%d, N(S)next=%d\n", handle,
        handle->peers[peer].i_queue_control.last_ns, handle->peers[peer].confirm_ns, handle->peers[peer].next_ns);
    }
}

///////////////////////////////////////////////////////////////////////////////