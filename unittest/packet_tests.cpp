/*
    Copyright 2019,2022 (C) Alexey Dynda

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

#include <functional>
#include <CppUTest/TestHarness.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "TinyPacket.h"

TEST_GROUP(PACKET){void setup(){
    // ...
}

                   void teardown(){
                       // ...
                   }};

TEST(PACKET, packet_write)
{
    const uint8_t array[] = {0x32, 'A', 0x24, 0x00, 0x32, 0x16, 0x00, 0x00, 0xFF, 0xFF,
                             'H',  'e', 'l',  'l',  'o',  0x00, 0x78, 0x56, 0x34, 0x12};
    tinyproto::StaticPacket<32> packet;
    tinyproto::StaticPacket<4> testPacket;
    testPacket.put((uint32_t)0x12345678);

    packet.put((uint8_t)0x32);
    packet.put('A');
    packet.put((uint16_t)0x24);
    packet.put((uint32_t)0x1632);
    packet.put((int16_t)(-1));
    packet.put("Hello");
    packet.put(testPacket);
    CHECK_EQUAL(32, packet.maxSize());
    CHECK_EQUAL(sizeof(array), packet.size());
    MEMCMP_EQUAL(array, packet.data(), sizeof(array));

    CHECK_EQUAL(0x32, packet.getByte());
    CHECK_EQUAL('A', packet.getChar());
    CHECK_EQUAL(0x24, packet.getUint16());
    CHECK_EQUAL(0x1632, packet.getUint32());
    CHECK_EQUAL(-1, packet.getInt16());
    STRCMP_EQUAL("Hello", packet.getString());
    CHECK_EQUAL(0x12345678, packet.getUint32());
    CHECK_EQUAL(0x32, packet[4]);
}
