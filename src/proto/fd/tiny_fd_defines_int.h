/*
    Copyright 2024-2025 (C) Alexey Dynda

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

#include "hal/tiny_debug.h"

#ifndef TINY_FD_DEBUG
#define TINY_FD_DEBUG 0
#endif

#if TINY_FD_DEBUG
#define LOG(lvl, fmt, ...) TINY_LOG(lvl, fmt, ##__VA_ARGS__)
// id - unique id of the protocol instance
// direction - direction of the log, can be "OUT" or "IN"
// addr - address of the peer, can be primary or secondary station address
// frame type - 'S', 'I' or 'U'
// subtype - subtype of the frame, can be "RR", "REJ", "UA", etc.
// ns - N(S) sequence number, nr - N(R) sequence number
#define FILE_LOG(id, direction, addr, frame, subtype, ns, nr) \
                TINY_FILE_LOG(id, "%s,   %02X,  %c, %s,    %d,   %d\n", direction, addr, frame, subtype, ns, nr)
#else
#define LOG(...)
#define FILE_LOG(...)
#endif

enum
{
    /**
     * FD_EVENT_TX_SENDING indicates that currently frame is being sent.
     * Higer layer will not try to send new frames until this event is cleared.
     */
    FD_EVENT_TX_SENDING = 0x01,            // Global event

    /**
     * FD_EVENT_TX_DATA_AVAILABLE indicates that there is data available for transmission.
     * This event is set when there is data in the TX queue.
     */
    FD_EVENT_TX_DATA_AVAILABLE = 0x02,     // Global event

    /**
     * FD_EVENT_QUEUE_HAS_FREE_SLOTS indicates that there are free slots in the TX queue.
     * This event is set when there is at least one free slot in the TX queue.
     * It is used to notify higher layers that they can send more data.
     */
    FD_EVENT_QUEUE_HAS_FREE_SLOTS = 0x04,  // Global event

    /**
     * FD_EVENT_CAN_ACCEPT_I_FRAMES indicates that the protocol can accept new I-frames for sending.
     * This event is set when the protocol is ready to accept new I-frames.
     */
    FD_EVENT_CAN_ACCEPT_I_FRAMES = 0x08,   // Local event

    /**
     * 
     */
    FD_EVENT_HAS_MARKER          = 0x10,   // Global event
};

#define HDLC_I_FRAME_BITS 0x00
#define HDLC_I_FRAME_MASK 0x01

#define HDLC_S_FRAME_BITS 0x01
#define HDLC_S_FRAME_MASK 0x03
#define HDLC_S_FRAME_TYPE_REJ 0x08
#define HDLC_S_FRAME_TYPE_RR 0x00
#define HDLC_S_FRAME_TYPE_RNR 0x04
#define HDLC_S_FRAME_TYPE_MASK 0x0C

#define HDLC_U_FRAME_BITS 0x03
#define HDLC_U_FRAME_MASK 0x03
// 2 lower bits of the command id's are zero, because they are covered by U_FRAME_BITS
#define HDLC_U_FRAME_TYPE_UA 0x60
#define HDLC_U_FRAME_TYPE_DM 0x0C
#define HDLC_U_FRAME_TYPE_FRMR 0x84
#define HDLC_U_FRAME_TYPE_RSET 0x8C
#define HDLC_U_FRAME_TYPE_SABM 0x2C
#define HDLC_U_FRAME_TYPE_SNRM 0x80
#define HDLC_U_FRAME_TYPE_DISC 0x40
#define HDLC_U_FRAME_TYPE_MASK 0xEC

#define HDLC_P_BIT 0x10
#define HDLC_F_BIT 0x10

// C/R bit is command / response bit 
// When this bit is set, it means that the frame is a command frame
// When this bit is clear, it means that the frame is a response frame
#define HDLC_CR_BIT 0x02
// Extension bit is used to indicate that the address field is extended
// If this bit is set, the address field is 1 byte long
// If this bit is clear, the address field is 2 bytes long
#define HDLC_E_BIT 0x01
#define HDLC_PRIMARY_ADDR (TINY_FD_PRIMARY_ADDR << 2)
#define HDLC_INVALID_PEER_INDEX 0xFF

static const uint8_t seq_bits_mask = 0x07;