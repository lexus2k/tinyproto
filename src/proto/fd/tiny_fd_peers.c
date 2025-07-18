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

#include "tiny_fd_peers_int.h"
#include "tiny_fd.h"
#include "tiny_fd_int.h"
#include "tiny_fd_defines_int.h"

///////////////////////////////////////////////////////////////////////////////

uint8_t __address_field_to_peer(tiny_fd_handle_t handle, uint8_t address)
{
    // Always clear C/R bit (0x00000010) when comparing addresses
    address &= (~HDLC_CR_BIT);
    // Exit, if extension bit is not set.
    if ( !(address & HDLC_E_BIT) )
    {
        // We do not support extended address format for now.
        return HDLC_INVALID_PEER_INDEX;
    }
    // If our station is SECONDARY station, then we must check that the frame is for us
    if ( __is_secondary_station( handle ) || handle->mode == TINY_FD_MODE_ABM )
    {
        // Check that the frame is for us
        return address == handle->addr ? 0 : HDLC_INVALID_PEER_INDEX;
    }
    // This code works only for primary station in NRM mode
    for ( uint8_t peer = 0; peer < handle->peers_count; peer++ )
    {
        if ( handle->peers[peer].addr == address )
        {
            return peer;
        }
    }
    // Return "NOT found peer"
    return HDLC_INVALID_PEER_INDEX;
}

///////////////////////////////////////////////////////////////////////////////

uint8_t __switch_to_next_peer(tiny_fd_handle_t handle)
{
    const uint8_t start_peer = handle->next_peer;
    do
    {
        if ( ++handle->next_peer >= handle->peers_count )
        {
            handle->next_peer = 0;
        }
        if ( handle->peers[ handle->next_peer ].addr != HDLC_INVALID_PEER_INDEX )
        {
            break;
        }
    } while ( start_peer != handle->next_peer );
    LOG(TINY_LOG_INFO, "[%p] Switching to peer [%02X]\n", handle, handle->next_peer);
    return start_peer != handle->next_peer;
}

///////////////////////////////////////////////////////////////////////////////

