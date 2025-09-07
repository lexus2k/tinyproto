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
#include "tiny_fd_defines_int.h"

#include <stdbool.h>

typedef struct i_queue_control_send_t
{
    uint8_t last_ns;     // next free frame to send in cycle buffer
    uint8_t confirm_ns;  // next sent frame to be confirmed
    uint8_t next_ns;     // next frame to be sent
} i_queue_control_send_t;

typedef struct i_queue_control_t
{
    struct i_queue_control_send_t tx_state;
} i_queue_control_t;

///////////////////////////////////////////////////////////////////////////////

static inline uint8_t __i_queue_control_get_next_frame_to_confirm(i_queue_control_t *control)
{
    return control->tx_state.confirm_ns;
}

///////////////////////////////////////////////////////////////////////////////

typedef bool (*on_frame_confirmed_cb_t)(void *ctx, uint8_t nr);

bool __i_queue_control_confirm_sent_frames(i_queue_control_t *control, uint8_t nr, on_frame_confirmed_cb_t cb, void *ctx);

///////////////////////////////////////////////////////////////////////////////

uint8_t __i_queue_control_get_next_frame_to_send(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

void __i_queue_control_move_to_previous_ns(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

void __i_queue_control_move_to_next_ns(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

bool __i_queue_control_has_unconfirmed_frames(i_queue_control_t *control);

///////////////////////////////////////////////////////////////////////////////

bool __all_frames_are_sent(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

uint32_t __time_passed_since_last_sent_i_frame(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

bool __can_accept_i_frames(i_queue_control_t *control);

///////////////////////////////////////////////////////////////////////////////

bool __put_i_frame_to_tx_queue(tiny_fd_handle_t handle, uint8_t peer, const void *data, int len);

///////////////////////////////////////////////////////////////////////////////

void __reset_i_queue_control(tiny_fd_handle_t handle, uint8_t peer);

///////////////////////////////////////////////////////////////////////////////

void __i_queue_control_log_statistics(tiny_fd_handle_t handle, uint8_t peer, uint8_t is_full);

///////////////////////////////////////////////////////////////////////////////
