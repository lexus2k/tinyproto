/*
    Copyright 2019-2022 (C) Alexey Dynda

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

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <profileapi.h>

void tiny_mutex_create(tiny_mutex_t *mutex)
{
    pthread_mutex_init(mutex, NULL);
}

void tiny_mutex_destroy(tiny_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

void tiny_mutex_lock(tiny_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

uint8_t tiny_mutex_try_lock(tiny_mutex_t *mutex)
{
    return pthread_mutex_trylock(mutex) == 0;
}

void tiny_mutex_unlock(tiny_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

void tiny_events_create(tiny_events_t *events)
{
    events->bits = 0;
    events->waiters = 0;

    pthread_mutex_init(&events->mutex, NULL);
    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    pthread_cond_init(&events->cond, &condattr);
}

void tiny_events_destroy(tiny_events_t *events)
{
    pthread_cond_destroy(&events->cond);
    pthread_mutex_destroy(&events->mutex);
}

uint8_t tiny_events_wait(tiny_events_t *events, uint8_t bits, uint8_t clear, uint32_t timeout)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000000ULL;
    if ( ts.tv_nsec >= 1000000000ULL )
    {
        ts.tv_nsec -= 1000000000ULL;
        ts.tv_sec++;
    }
    pthread_mutex_lock(&events->mutex);
    events->waiters++;
    int res = 0;
    while ( (events->bits & bits) == 0 )
    {
        if ( timeout == 0xFFFFFFFF )
        {
            pthread_cond_wait(&events->cond, &events->mutex);
        }
        else
        {
            res = pthread_cond_timedwait(&events->cond, &events->mutex, &ts);
            if ( res == ETIMEDOUT )
            {
                break;
            }
        }
    }
    uint8_t locked = 0;
    if ( res != ETIMEDOUT )
    {
        locked = events->bits;
        if ( clear )
            events->bits &= ~bits;
    }
    events->waiters--;
    pthread_mutex_unlock(&events->mutex);
    return locked;
}

uint8_t tiny_events_check_int(tiny_events_t *event, uint8_t bits, uint8_t clear)
{
    return tiny_events_wait(event, bits, clear, 0);
}

void tiny_events_set(tiny_events_t *events, uint8_t bits)
{
    pthread_mutex_lock(&events->mutex);
    events->bits |= bits;
    pthread_cond_broadcast(&events->cond);
    pthread_mutex_unlock(&events->mutex);
}

void tiny_events_clear(tiny_events_t *events, uint8_t bits)
{
    pthread_mutex_lock(&events->mutex);
    events->bits &= ~bits;
    pthread_mutex_unlock(&events->mutex);
}

void tiny_sleep(uint32_t millis)
{
    usleep(millis * 1000);
}

void tiny_sleep_us(uint32_t us)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * (__int64)us);

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

uint32_t tiny_millis()
{
    return GetTickCount();
}

uint32_t tiny_micros()
{
    LARGE_INTEGER T;
    LARGE_INTEGER F;

    QueryPerformanceFrequency(&F);
    QueryPerformanceCounter(&T);

    T.QuadPart = T.QuadPart * 1000000 / F.QuadPart;
    return (uint32_t)T.QuadPart;
}
