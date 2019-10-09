/*
    Copyright 2017 (C) Alexey Dynda

    This file is part of Tiny Protocol Library.

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
*/

#ifndef _FAKE_WIRE_H_
#define _FAKE_WIRE_H_

#include <stdint.h>
#include <pthread.h>
#include <mutex>
#include <list>

class FakeWire
{
public:
    FakeWire();
    ~FakeWire();
    int read(uint8_t * data, int length);
    int write(const uint8_t * data, int length);
    void generate_error_every_n_byte(int n) { m_errors.push_back( ErrorDesc{0, n, -1} ); };
    void generate_single_error(int position) { m_errors.push_back( ErrorDesc{position, position, 1} ); };
    void generate_error(int first, int period, int count = -1) { m_errors.push_back( ErrorDesc{first, period, count} ); };
    void reset();
private:
    typedef struct
    {
        int first;
        int period;
        int count;
    } ErrorDesc;
    int          m_writeptr;
    int          m_readptr;
    std::mutex   m_mutex{};
    uint8_t      m_buf[1024];
    std::list<ErrorDesc> m_errors;
    int          m_byte_counter = 0;
};


#endif /* _FAKE_WIRE_H_ */
