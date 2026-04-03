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

#include "tiny_fd.h"
#include "hal/tiny_debug.h"
#include "tiny_fd_int.h"
#include "tiny_fd_defines_int.h"

///////////////////////////////////////////////////////////////////////////////

static tiny_fd_frame_type_t __get_frame_type(uint8_t control)
{
    if ((control & HDLC_I_FRAME_MASK) == HDLC_I_FRAME_BITS)
    {
        return TINY_FD_FRAME_TYPE_I;
    }
    else if ((control & HDLC_S_FRAME_MASK) == HDLC_S_FRAME_BITS)
    {
        return TINY_FD_FRAME_TYPE_S;
    }
    return TINY_FD_FRAME_TYPE_U;
}

///////////////////////////////////////////////////////////////////////////////

static tiny_fd_frame_subtype_t __get_frame_subtype(uint8_t control)
{
    tiny_fd_frame_type_t type = __get_frame_type(control);
    if (type == TINY_FD_FRAME_TYPE_I) {
        // I-frames do not have subtypes
        return TINY_FD_FRAME_SUBTYPE_RR;
    }
    else if (type == TINY_FD_FRAME_TYPE_S) {
        return (tiny_fd_frame_subtype_t)(control & HDLC_S_FRAME_TYPE_MASK);
    }
    return (tiny_fd_frame_subtype_t)(control & HDLC_U_FRAME_TYPE_MASK);
}

///////////////////////////////////////////////////////////////////////////////

static uint8_t __get_frame_sequence(uint8_t control)
{
    tiny_fd_frame_type_t type = __get_frame_type(control);
    switch (type)
    {
        case TINY_FD_FRAME_TYPE_I: return (control >> 1) & 0x07;
        case TINY_FD_FRAME_TYPE_S:
        case TINY_FD_FRAME_TYPE_U:
        default: return 0;
    }
}

///////////////////////////////////////////////////////////////////////////////

static uint8_t __get_awaiting_sequence(uint8_t control)
{
    tiny_fd_frame_type_t type = __get_frame_type(control);
    switch (type)
    {
        case TINY_FD_FRAME_TYPE_I: return control >> 5;
        case TINY_FD_FRAME_TYPE_S: return control >> 5;
        case TINY_FD_FRAME_TYPE_U:
        default: return 0;
    }
}

#if defined(TINY_FD_DEBUG) && defined(TINY_FILE_LOGGING)
///////////////////////////////////////////////////////////////////////////////

static const char __get_frame_type_str(uint8_t control)
{
    tiny_fd_frame_type_t type = __get_frame_type(control);
    switch (type)
    {
        case TINY_FD_FRAME_TYPE_I: return 'I';
        case TINY_FD_FRAME_TYPE_S: return 'S';
        case TINY_FD_FRAME_TYPE_U: return 'U';
        default: return 'U';
    }
}

///////////////////////////////////////////////////////////////////////////////

static const char *__get_frame_subtype_str(uint8_t control)
{
    tiny_fd_frame_type_t type = __get_frame_type(control);
    if (type == TINY_FD_FRAME_TYPE_I) {
        return "    ";
    }
    else if (type == TINY_FD_FRAME_TYPE_S) {
        switch (control & HDLC_S_FRAME_TYPE_MASK)
        {
            case HDLC_S_FRAME_TYPE_RR:  return "  RR";
            case HDLC_S_FRAME_TYPE_RNR: return " RNR";
            case HDLC_S_FRAME_TYPE_REJ: return " REJ";
            case HDLC_S_FRAME_TYPE_SREJ: return "SREJ";
            default:                    return " UNK";
        }
    }
    switch (control & HDLC_U_FRAME_TYPE_MASK)
    {
        case HDLC_U_FRAME_TYPE_UA:   return "  UA";
        case HDLC_U_FRAME_TYPE_DM:   return "  DM";
        case HDLC_U_FRAME_TYPE_FRMR: return "FRMR";
        case HDLC_U_FRAME_TYPE_RSET: return "RSET";
        case HDLC_U_FRAME_TYPE_SABM: return "SABM";
        case HDLC_U_FRAME_TYPE_SNRM: return "SNRM";
        case HDLC_U_FRAME_TYPE_DISC: return "DISC";
        case HDLC_U_FRAME_TYPE_UI:   return "  UI";
        default:                     return " UNK";
    }
}

#endif // TINY_FD_DEBUG && TINY_FILE_LOGGING

void __tiny_fd_log_frame(tiny_fd_handle_t handle,
                       tiny_fd_frame_direction_t direction,
                       const uint8_t *data,
                       int len)
{
    if (handle == NULL || data == NULL || len < 2) {
        return;
    }
    if (handle->log_frame_cb) {
        handle->log_frame_cb(handle->user_data,
                             handle,
                             direction,
                             __get_frame_type(data[1]),
                             __get_frame_subtype(data[1]),
                             __get_frame_sequence(data[1]),
                             __get_awaiting_sequence(data[1]), data, len);
    }
    FILE_LOG((uintptr_t)handle,
        direction == TINY_FD_FRAME_DIRECTION_IN ? " IN" : "OUT",
        data[0],
        __get_frame_type_str(data[1]),
        __get_frame_subtype_str(data[1]),
        __get_frame_sequence(data[1]),
        __get_awaiting_sequence(data[1]));
}

