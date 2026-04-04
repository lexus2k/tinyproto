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

#if defined(ARDUINO)
#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#endif

#include "TinyLightProtocol.h"

namespace tinyproto
{
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

void Light::begin(write_block_cb_t writecb, read_block_cb_t readcb)
{
    m_data.crc_type = m_crc;
    tiny_light_init(&m_data, writecb, readcb, this);
}

void Light::end()
{
    tiny_light_close(&m_data);
}

int Light::write(char *buf, int size)
{
    return tiny_light_send(&m_data, (uint8_t *)buf, size);
}

int Light::read(char *buf, int size)
{
    return tiny_light_read(&m_data, (uint8_t *)buf, size);
}

int Light::write(const IPacket &pkt)
{
    return tiny_light_send(&m_data, pkt.m_buf, pkt.m_len);
}

int Light::read(IPacket &pkt)
{
    int len = tiny_light_read(&m_data, pkt.m_buf, pkt.m_size);
    pkt.m_p = 0;
    pkt.m_len = len;
    return len;
}

void Light::disableCrc()
{
    m_crc = HDLC_CRC_OFF;
}

void Light::enableCrc(hdlc_crc_t crc)
{
    m_crc = crc;
}

bool Light::enableCheckSum()
{
#if defined(CONFIG_ENABLE_CHECKSUM)
    m_crc = HDLC_CRC_8;
    return true;
#else
    return false;
#endif
}

bool Light::enableCrc16()
{
#if defined(CONFIG_ENABLE_FCS16)
    m_crc = HDLC_CRC_16;
    return true;
#else
    return false;
#endif
}

bool Light::enableCrc32()
{
#if defined(CONFIG_ENABLE_FCS32)
    m_crc = HDLC_CRC_32;
    return true;
#else
    return false;
#endif
}

#ifdef ARDUINO

static int writeToSerial(void *p, const void *b, int s)
{
    int result = Serial.write((const uint8_t *)b, s);
    return result;
}

static int readFromSerial(void *p, void *b, int s)
{
    int length = Serial.readBytes((uint8_t *)b, s);
    return length;
}

void Light::beginToSerial()
{
    Serial.setTimeout(100);
    begin(writeToSerial, readFromSerial);
}
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

} // namespace tinyproto
