/*
    Copyright 2021-2025 (C) Alexey Dynda

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

#ifdef __cplusplus
extern "C"
{
#endif

#include "hal/tiny_types.h"
#include <stdint.h>
#include <stdbool.h>

    typedef enum
    {
        TINY_FD_QUEUE_FREE = 0x01,
        TINY_FD_QUEUE_U_FRAME = 0x02,
        TINY_FD_QUEUE_S_FRAME = 0x04,
        TINY_FD_QUEUE_I_FRAME = 0x08
    } tiny_fd_queue_type_t;

    typedef struct
    {
        uint8_t address;  // address field of HDLC protocol
        uint8_t control;  // control field of HDLC protocol
    } tiny_frame_header_t;

    typedef struct
    {
        uint8_t type; ///< tiny_fd_queue_type_t value
        int len;      ///< payload of the frame
        /* Aligning header to 1 byte, since header and user_payload together are the byte-stream */
        TINY_ALIGNED(1) tiny_frame_header_t header; ///< header, fill every time, when user payload is sending
        uint8_t payload[2];       ///< this byte and all bytes after are user payload
    } tiny_fd_frame_info_t;

#ifdef __cplusplus
}
#endif

#endif
