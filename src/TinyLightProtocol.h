/*
    Copyright 2017-2019,2022 (C) Alexey Dynda

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
 This is Tiny Light protocol C++ wrapper implementation.

 @file
 @brief Tiny Light protocol C++ API
*/

#ifndef _TINY_LIGHT_PROTOCOL_H_
#define _TINY_LIGHT_PROTOCOL_H_

#include "TinyPacket.h"
#include "proto/light/tiny_light.h"

#ifdef ARDUINO
#include <HardwareSerial.h>
#else
#include <string.h>
#endif

namespace tinyproto
{

/**
 * @ingroup LIGHT_API
 * @{
 */

/**
 *  Light class encapsulates the Light protocol functionality.
 *  It provides simple SLIP-like framing with optional CRC.
 *  No acknowledgments or retransmission — use it when simplicity
 *  and low overhead are more important than guaranteed delivery.
 *
 *  You may also use the C-style API functions (tiny_light_*) directly.
 */
class Light
{
public:
    inline Light()
        : m_data{}
    {
    }

    /**
     * Initializes protocol internal variables.
     * If you need to switch communication with other destination
     * point, you can call this method one again after calling end().
     * @param writecb - write function to some physical channel
     * @param readcb  - read function from some physical channel
     * @return None
     */
    void begin(write_block_cb_t writecb, read_block_cb_t readcb);

#ifdef ARDUINO
    /**
     * Initializes protocol internal variables and redirects
     * communication through Arduino Serial connection (Serial).
     * @return None
     */
    void beginToSerial();

#ifdef HAVE_HWSERIAL1
    /**
     * Initializes protocol internal variables and redirects
     * communication through Arduino Serial1 connection (Serial1).
     * @return None
     */
    inline void beginToSerial1()
    {
        begin([](void *p, const void *b, int s) -> int { return Serial1.write((const uint8_t *)b, s); },
              [](void *p, void *b, int s) -> int { return Serial1.readBytes((uint8_t *)b, s); });
    }
#endif

#ifdef HAVE_HWSERIAL2
    /**
     * Initializes protocol internal variables and redirects
     * communication through Arduino Serial2 connection (Serial2).
     * @return None
     */
    inline void beginToSerial2()
    {
        begin([](void *p, const void *b, int s) -> int { return Serial2.write((const uint8_t *)b, s); },
              [](void *p, void *b, int s) -> int { return Serial2.readBytes((uint8_t *)b, s); });
    }
#endif

#ifdef HAVE_SERIALUSB
    /**
     * Initializes protocol internal variables and redirects
     * communication through Arduino Serial1 connection (SerialUSB).
     * @return None
     */
    inline void beginToSerialUSB()
    {
        begin([](void *p, const void *b, int s) -> int { return SerialUSB.write((const char *)b, s); },
              [](void *p, void *b, int s) -> int { return SerialUSB.readBytes((char *)b, s); });
    }
#endif

#endif

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
    int write(char *buf, int size);

    /**
     * Reads data block from communication channel.
     * @param buf - buffer to place data read from communication channel
     * @param size - maximum size of the buffer in bytes.
     * @return negative value in case of error
     *         zero if nothing is read
     *         positive - number of bytes read from the channel
     */
    int read(char *buf, int size);

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
     * Reads packet from communication channel.
     * @param pkt - Packet object to put data to
     * @see Packet
     * @return negative value in case of error
     *         zero if nothing is read
     *         positive - Packet is successfully received
     */
    int read(IPacket &pkt);

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

private:
    STinyLightData m_data{};

    hdlc_crc_t m_crc = HDLC_CRC_DEFAULT;
};

/**
 * @}
 */

} // namespace tinyproto

#endif
