/*
    Copyright 2021-2024 (C) Alexey Dynda

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

#include "TinyProtocol.h"

namespace tinyproto
{

enum
{
    PROTO_RX_MESSAGE = 1,
};

Proto::Proto(bool multithread)
   : m_link(nullptr)
   , m_multithread( multithread )
{
    tiny_events_create( &m_events );
    tiny_mutex_create( &m_mutex );
}

Proto::~Proto()
{
    end();
    tiny_mutex_destroy( &m_mutex );
    tiny_events_destroy( &m_events );
}

void Proto::setLink(ILinkLayer &link)
{
    m_link = &link;
}

ILinkLayer &Proto::getLink()
{
    return *m_link;
}

bool Proto::begin()
{
    uint32_t timeout = m_link->getTimeout();
    if ( m_multithread && !timeout )
    {
        timeout = 1000;
    }
    else if ( !m_multithread)
    {
        timeout = 0;
    }
    m_link->setTimeout( timeout );
    if (!m_link->begin(onReadCb, onSendCb, this))
    {
        m_terminate = true;
        return false;
    }
#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
    if ( m_multithread )
    {
        m_terminate = false;
        m_readThread = new std::thread(&Proto::runRx, this);
        m_sendThread = new std::thread(&Proto::runTx, this);
    }
#endif
    return true;
}

bool Proto::begin(int poolBuffers)
{
    return Proto::begin();
}

bool Proto::send(const IPacket &packet, uint32_t timeout)
{
    bool result = false;
    uint32_t startTs = tiny_millis();
    for ( ;; )
    {
        // Try to put message to outgoing queue
        result = m_link->put( packet.m_buf, packet.m_len, m_multithread ? timeout : 0 );
        if ( result )
        {
            break;
        }
        if ( static_cast<uint32_t>(tiny_millis() - startTs) >= timeout )
        {
            // Cancel send operation if possible
            m_link->flushTx();
            break;
        }
        if ( !m_multithread )
        {
            m_link->runTx();
            m_link->runRx();
        }
    }
    return result;
}

/*void Proto::printCount(const char * name, IPacket *queue)
{
    int counter = 0;
    IPacket *p = queue;
    while ( p )
    {
        p = p->m_next;
        counter++;
    }
    printf("[%p] %s counter %i \n", this, name, counter);
}*/


IPacket *Proto::read(uint32_t timeout)
{
    IPacket *p = nullptr;
    uint32_t startTs = tiny_millis();
    for ( ;; )
    {
        tiny_events_wait(&m_events, PROTO_RX_MESSAGE, EVENT_BITS_CLEAR, m_multithread ? timeout : 0);
        tiny_mutex_lock( &m_mutex );
        if ( m_queue != nullptr )
        {
            p = m_queue;
            m_queue = m_queue->m_next;
            if ( m_queue != nullptr )
            {
                m_queue->m_prev = nullptr;
                tiny_events_set( &m_events, PROTO_RX_MESSAGE );
            }
            else
            {
                m_last = nullptr;
            }
            //printCount( "read Pool", m_pool );
            //printCount( "read Queue", m_queue );
            tiny_mutex_unlock( &m_mutex );
            break;
        }
        tiny_mutex_unlock( &m_mutex );
        // Always run Tx/Rx loop before checking timings, otherwise messages will be never received
        if ( !m_multithread )
        {
            m_link->runTx();
            m_link->runRx();
        }
        if ( static_cast<uint32_t>(tiny_millis() - startTs) >= timeout )
        {
            break;
        }
    }
    return p;
}

void Proto::end()
{
    if ( m_terminate )
    {
        return;
    }
    m_terminate = true;
#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
    if ( m_sendThread )
    {
        m_sendThread->join();
        m_sendThread = nullptr;
    }
    if ( m_readThread )
    {
        m_readThread->join();
        m_readThread = nullptr;
    }
#endif
    m_link->end();
    return;
}

void Proto::onRead(uint8_t addr, uint8_t *buf, int len)
{
    // We do not need pool for callback mode
    if ( m_onRx )
    {
        IPacket packet((char *)buf, len);
        packet.m_len = len;
        m_onRx(*this, packet);
        return;
    }
    tiny_mutex_lock( &m_mutex );
    IPacket * p = m_pool;
    if ( p == nullptr )
    {
#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
        // TODO: Lost frame
        m_lostRxFrames++;
        // printf("Lost -------------------- \n");
#endif
    }
    else
    {
        // Remove from pool
        m_pool = m_pool->m_next;
        if ( m_pool )
        {
            m_pool->m_prev = nullptr;
        }
        // Add to rx queue
        p->m_next = nullptr;
        p->m_prev = m_last;
        if ( m_last )
        {
            m_last->m_next = p;
        }
        m_last = p;
        if ( m_queue == nullptr )
        {
            m_queue = p;
        }
        // Copy data if needed
        if ( p->m_size == 0 )
        {
            p->m_buf = buf;
            p->m_len = len;
        }
        else
        {
            // TODO: Error if oversize
            p->m_len = p->m_size < len ? p->m_size: len;
            memcpy( p->m_buf, buf, p->m_len );
        }
        p->m_p = 0;
        tiny_events_set( &m_events, PROTO_RX_MESSAGE );
    }
    //printCount( "new Pool", m_pool );
    //printCount( "new Queue", m_queue );
    tiny_mutex_unlock( &m_mutex );
}

void Proto::onSend(uint8_t addr, const uint8_t *buf, int len)
{
    if ( m_onTx )
    {
        IPacket packet((char *)buf, len);
        packet.m_len = len;
        m_onTx(*this, packet);
    }    
}

void Proto::onReadCb(void *udata, uint8_t addr, uint8_t *buf, int len)
{
    Proto *proto = reinterpret_cast<Proto *>(udata);
    proto->onRead(addr, buf, len);
}

void Proto::onSendCb(void *udata, uint8_t addr, const uint8_t *buf, int len)
{
    Proto *proto = reinterpret_cast<Proto *>(udata);
    proto->onSend(addr, buf, len);
}

#if CONFIG_TINYHAL_THREAD_SUPPORT == 1
void Proto::runTx()
{
    if (m_multithread)
    {
        if (m_txDelay)
        {
            tiny_sleep( m_txDelay );
        }
        while ( !m_terminate )
        {
            getLink().runTx();
        }
    }
}

void Proto::runRx()
{
    if (m_multithread)
    {
        while ( !m_terminate )
        {
            getLink().runRx();
        }
    }
}

void Proto::setTxDelay( uint32_t delay )
{
    m_txDelay = delay;
}

int Proto::getLostRxFrames()
{
    int val = m_lostRxFrames;
    m_lostRxFrames = 0;
    return val;
}
#endif

void Proto::release(IPacket *message)
{
    addRxPool(*message);
}

void Proto::addRxPool(IPacket &message)
{
    tiny_mutex_lock( &m_mutex );
    message.m_next = m_pool;
    message.m_prev = nullptr;
    if ( m_pool != nullptr )
    {
        m_pool->m_prev = &message;
    }
    m_pool = &message;
    //printCount( "release Pool", m_pool );
    //printCount( "release Queue", m_queue );
    tiny_mutex_unlock( &m_mutex );
}

void Proto::setRxCallback(void (*onRx)(Proto &, IPacket &))
{
    m_onRx = onRx;
}

void Proto::setTxCallback(void (*onTx)(Proto &, IPacket &))
{
    m_onTx = onTx;
}

//////////// Platform specific helper classes

#if defined(ARDUINO)

SerialFdProto::SerialFdProto(HardwareSerial &port)
    : Proto( false )
    , m_layer( &port )
{
    setLink( m_layer );
}

ArduinoSerialFdLink &SerialFdProto::getLink()
{
    return m_layer;
}

SerialHdlcProto::SerialHdlcProto(HardwareSerial &port)
    : Proto( false )
    , m_layer( &port )
{
    setLink( m_layer );
}

ArduinoSerialHdlcLink &SerialHdlcProto::getLink()
{
    return m_layer;
}

#else

SerialFdProto::SerialFdProto(char *dev, bool multithread)
    : Proto( multithread )
    , m_layer( dev )
{
    setLink( m_layer );
}

SerialFdLink &SerialFdProto::getLink()
{
    return m_layer;
}

SerialHdlcProto::SerialHdlcProto(char *dev, bool multithread)
    : Proto( multithread )
    , m_layer( dev )
{
    setLink( m_layer );
}

SerialHdlcLink &SerialHdlcProto::getLink()
{
    return m_layer;
}

#endif

} // namespace tinyproto
