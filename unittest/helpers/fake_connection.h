/*
    Copyright 2019-2020,2022 (C) Alexey Dynda

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

#include "fake_wire.h"
#include "fake_endpoint.h"
#include <atomic>
#include <list>

class FakeSetup
{
public:
    FakeSetup();
    FakeSetup(int p1_hw_size, int p2_hw_size);

    ~FakeSetup();

    FakeEndpoint &endpoint1();
    FakeEndpoint &endpoint2();

    FakeWire &line1()
    {
        return m_line1;
    }

    FakeWire &line2()
    {
        return m_line2;
    }

    void setSpeed(uint32_t bps);

    int lostBytes();

private:
    FakeWire m_line1{};
    FakeWire m_line2{};
    std::list<FakeEndpoint *> m_endpoints{};

    std::atomic<uint64_t> m_interval_us{50};
    std::atomic<uint64_t> m_Bps{64000};
    std::atomic<bool> m_stopped{false};
    std::thread *m_line_thread = nullptr;

    static void TransferDataStatic(FakeSetup *conn);
    void TransferData();
};
