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

#pragma once

#include "hal/tiny_types.h"

#include <cstdint>
#include <mutex>
#include <chrono>
#include <condition_variable>

template <uint32_t M> class Semaphore
{
public:
    Semaphore(uint32_t count)
        : m_count(count)
    {
    }

    void acquire()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [&]() -> bool { return m_count > 0; });
        m_count--;
    }

    bool try_acquire()
    {
        return try_acquire_for(std::chrono::milliseconds(0));
    }

    template <typename R, typename P> bool try_acquire_for(const std::chrono::duration<R, P> &timeout)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if ( !m_condition.wait_for(lock, timeout, [&]() -> bool { return m_count > 0; }) )
        {
            return false;
        }
        m_count--;
        return true;
    }

    void release()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if ( m_count < m_max )
        {
            m_count++;
        }
        lock.unlock();
        m_condition.notify_one();
    }

private:
    uint32_t m_count;
    uint32_t m_max = M;
    std::mutex m_mutex{};
    std::condition_variable m_condition{};
};

class BinarySemaphore: public Semaphore<1>
{
public:
    BinarySemaphore(uint32_t value)
        : Semaphore(value ? 1 : 0)
    {
    }
};
