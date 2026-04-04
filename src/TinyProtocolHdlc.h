/*
    Copyright 2020,2022 (C) Alexey Dynda

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

/**
 This is Tiny protocol HDLC C++ wrapper implementation.

 @file
 @brief Tiny Protocol HDLC C++ API

*/
#pragma once

#include "TinyPacket.h"
#include "proto/hdlc/high_level/hdlc.h"

#ifdef ARDUINO
#include <HardwareSerial.h>
#else
#include <string.h>
#endif

namespace tinyproto
{

/**
 * @ingroup HDLC_API
 * @{
 */

/**
 *  Hdlc class encapsulates HDLC low-level framing functionality.
 *  It handles flag bytes (0x7E), byte stuffing, and CRC validation.
 *  This class provides raw HDLC framing — it does not perform
 *  acknowledgments or retransmission. For reliable delivery,
 *  use the Full Duplex protocol (IFd / Fd<N> / FdD).
 *
 *  You may also use the C-style API functions (hdlc_*) directly.
 */
class Hdlc
{
public:
    /**
     * Initializes Hdlc object
     * @param buffer - buffer to store the frames being received.
     * @param bufferSize - size of the buffer
     */
    Hdlc(void *buffer, int bufferSize)
        : m_buffer(buffer)
        , m_bufferSize(bufferSize)
    {
    }

    virtual ~Hdlc() = default;

    /**
     * Initializes protocol internal variables.
     * If you need to switch communication with other destination
     * point, you can call this method one again after calling end().
     * @param writecb - write function to some physical channel
     * @param readcb  - read function from some physical channel
     * @return None
     */
    void begin(write_block_cb_t writecb, read_block_cb_t readcb);

    /**
     * Initializes protocol internal variables.
     * If you need to switch communication with other destination
     * point, you can call this method one again after calling end().
     * @return None
     */
    void begin();

    /**
     * Resets protocol state.
     */
    void end();

    /**
     * Sends data block over communication channel.
     * @param buf - data to send
     * @param size - length of the data in bytes
     * @return negative value in case of error
     *         zero if nothing is sent
     *         positive - should be equal to size parameter
     */
    int write(const char *buf, int size);

    /**
     * Sends packet over communication channel.
     * @param pkt - Packet to send
     * @see Packet
     * @return negative value in case of error
     *         zero if nothing is sent
     *         positive - Packet is successfully sent
     */
    int write(const IPacket &pkt);

    /**
     * Processes incoming rx data, specified by a user.
     * @return TINY_SUCCESS
     * @remark if Packet is receive during run execution
     *         callback is called.
     */
    int run_rx(const void *data, int len);

    /**
     * Generates data for tx channel
     * @param data buffer to fill
     * @param max_len length of buffer
     * @return number of bytes generated
     */
    int run_tx(void *data, int max_len);

    /**
     * Disable CRC field in the protocol.
     * If CRC field is OFF, then the frame looks like this:
     * 0x7E databytes 0x7E.
     */
    void disableCrc();

    /**
     * Enables CRC by specified bit-size.
     * 8-bit is supported by Nano version of Tiny library.
     * @param crc crc type
     */
    void enableCrc(hdlc_crc_t crc);

    /**
     * Enables CRC 8-bit field in the protocol. This field
     * contains sum of all data bytes in the packet.
     * 8-bit field is supported by Nano version of Tiny library.
     * @return true if successful
     *         false in case of error.
     */
    bool enableCheckSum();

    /**
     * Enables CRC 16-bit field in the protocol. This field
     * contains FCS 16-bit CCITT like defined in RFC 1662.
     * 16-bit field is not supported by Nano version of Tiny library.
     * @return true if successful
     *         false in case of error.
     */
    bool enableCrc16();

    /**
     * Enables CRC 32-bit field in the protocol. This field
     * contains FCS 32-bit CCITT like defined in RFC 1662.
     * 32-bit field is not supported by Nano version of
     * Tiny library.
     * @return true if successful
     *         false in case of error.
     */
    bool enableCrc32();

    /**
     * Sets receive callback for incoming messages
     * @param on_receive user callback to process incoming messages. The processing must be non-blocking
     */
    void setReceiveCallback(void (*on_receive)(IPacket &pkt) = nullptr)
    {
        m_onReceive = on_receive;
    };

    /**
     * Sets send callback for outgoing messages
     * @param on_send user callback to process outgoing messages. The processing must be non-blocking
     */
    void setSendCallback(void (*on_send)(IPacket &pkt) = nullptr)
    {
        m_onSend = on_send;
    };

protected:
    /**
     * Method called by hdlc protocol upon receiving new frame.
     * Can be redefined in derived classes.
     * @param pdata pointer to received data
     * @param size size of received payload in bytes
     */
    virtual void onReceive(uint8_t *pdata, int size)
    {
        IPacket pkt((char *)pdata, size);
        pkt.m_len = size;
        if ( m_onReceive )
            m_onReceive(pkt);
    }

    /**
     * Method called by hdlc protocol upon sending next frame.
     * Can be redefined in derived classes.
     * @param pdata pointer to sent data
     * @param size size of sent payload in bytes
     */
    virtual void onSend(const uint8_t *pdata, int size)
    {
        IPacket pkt((char *)pdata, size);
        pkt.m_len = size;
        if ( m_onSend )
            m_onSend(pkt);
    }

private:
    /** The variable contain protocol state */
    hdlc_handle_t m_handle = nullptr;

    hdlc_struct_t m_data{};

    void *m_buffer = nullptr;

    int m_bufferSize = 0;

    hdlc_crc_t m_crc = HDLC_CRC_DEFAULT;

    /** Callback, when new frame is received */
    void (*m_onReceive)(IPacket &pkt) = nullptr;

    /** Callback, when new frame is sent */
    void (*m_onSend)(IPacket &pkt) = nullptr;

    /** Internal function */
    static int onReceiveInternal(void *handle, void *pdata, int size);

    /** Internal function */
    static int onSendInternal(void *handle, const void *pdata, int size);
};

/**
 * @}
 */

} // namespace tinyproto
