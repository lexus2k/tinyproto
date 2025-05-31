/*
    Copyright 2016-2024 (C) Alexey Dynda

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
 This is Tiny protocol implementation for microcontrollers

 @file
 @brief Tiny protocol Arduino API

*/

#pragma once

#include "TinyPacket.h"
#include "TinyLightProtocol.h"
#include "TinyProtocolHdlc.h"
#include "TinyProtocolFd.h"
#include "link/TinyLinkLayer.h"
#include "link/TinySerialFdLink.h"
#include "link/TinySerialHdlcLink.h"

#include "hal/tiny_types.h"

#include <stdint.h>
#include <limits.h>

#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
#include <thread>
#endif

namespace tinyproto
{

class Proto
{
public:
    explicit Proto(bool multithread = false);

    ~Proto();

    void setLink(ILinkLayer &link);

    ILinkLayer &getLink();

    bool begin();

    bool begin(int poolBuffers);

    bool send(const IPacket &message, uint32_t timeout);

    IPacket *read(uint32_t timeout);

    void end();

    void release(IPacket *message);

    void addRxPool(IPacket &message);

    void setRxCallback(void (*onRx)(Proto &, IPacket &));

    void setTxCallback(void (*onTx)(Proto &, IPacket &));

#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
    void setTxDelay( uint32_t delay );

    int getLostRxFrames();
#endif

private:
    ILinkLayer *m_link = nullptr;
    void (*m_onRx)(Proto &, IPacket &) = nullptr;
    void (*m_onTx)(Proto &, IPacket &) = nullptr;
    bool m_multithread = false;
    bool m_terminate = true;
    IPacket *m_pool = nullptr;
    IPacket *m_queue = nullptr;
    IPacket *m_last = nullptr;
#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
    std::thread *m_sendThread = nullptr;
    std::thread *m_readThread = nullptr;
    uint32_t m_txDelay = 0;
    int m_lostRxFrames = 0;
#endif

    tiny_events_t m_events{};

    tiny_mutex_t m_mutex{};

    void onRead(uint8_t addr, uint8_t *buf, int len);

    void onSend(uint8_t addr, const uint8_t *buf, int len);

    static void onReadCb(void *udata, uint8_t addr, uint8_t *buf, int len);

    static void onSendCb(void *udata, uint8_t addr, const uint8_t *buf, int len);

#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
    void runTx();

    void runRx();
#endif

//    void printCount(const char *, IPacket *queue);
};

/////// Helper classes, platform specific

#if defined(ARDUINO)

class SerialFdProto: public Proto
{
public:
    explicit SerialFdProto(HardwareSerial &port);

    ArduinoSerialFdLink &getLink();

private:
    ArduinoSerialFdLink m_layer;
};

class SerialHdlcProto: public Proto
{
public:
    explicit SerialHdlcProto(HardwareSerial &port);

    ArduinoSerialHdlcLink &getLink();

private:
    ArduinoSerialHdlcLink m_layer;
};

#else

class SerialFdProto: public Proto
{
public:
    explicit SerialFdProto(char *dev, bool multithread = false);

    SerialFdLink &getLink();

private:
    SerialFdLink m_layer;
};

class SerialHdlcProto: public Proto
{
public:
    explicit SerialHdlcProto(char *dev, bool multithread = false);

    SerialHdlcLink &getLink();

private:
    SerialHdlcLink m_layer;
};

#endif

} // namespace tinyproto
