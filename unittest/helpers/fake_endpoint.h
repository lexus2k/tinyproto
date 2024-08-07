/*
    Copyright 2017-2024 (C) Alexey Dynda

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

#ifndef _FAKE_CHANNEL_H_
#define _FAKE_CHANNEL_H_

#include <stdint.h>
#include "fake_wire.h"

class FakeEndpoint
{
public:
    // This constructor creates looped device
    FakeEndpoint(FakeWire &both, int rxSize, int txSize);

    // This constructor creates normal device with TX and RX lines
    FakeEndpoint(FakeWire &tx, FakeWire &rx, int rxSize, int txSize);

    ~FakeEndpoint();

    int read(uint8_t *data, int length);

    int write(const uint8_t *data, int length);

    bool wait_until_rx_count(int count, int timeout);

    void setTimeout(int timeout_ms)
    {
        m_timeout = timeout_ms;
    }
    void disable()
    {
        m_rxBlock->Disable();
    }
    void enable()
    {
        m_rxBlock->Enable();
    }
    void flush()
    {
        m_rxBlock->Flush();
    }

private:
    FakeWire &m_tx;
    FakeWire &m_rx;
    TxHardwareBlock *m_txBlock = nullptr;
    RxHardwareBlock *m_rxBlock = nullptr;
    std::mutex m_mutex;
    int m_timeout = 10;
};

#endif /* _FAKE_WIRE_H_ */
