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

#pragma once

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <mach/mach_time.h>

#ifndef CONFIG_ENABLE_CHECKSUM
#define CONFIG_ENABLE_CHECKSUM
#endif

#ifndef CONFIG_ENABLE_FCS16
#define CONFIG_ENABLE_FCS16
#endif

#ifndef CONFIG_ENABLE_FCS32
#define CONFIG_ENABLE_FCS32
#endif

#define CONFIG_TINYHAL_THREAD_SUPPORT 1

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**
 * Mutex type used by Tiny Protocol implementation.
 * The type declaration depends on platform.
 */
typedef pthread_mutex_t tiny_mutex_t;

/**
 * Events group type used by Tiny Protocol implementation.
 * The type declaration depends on platform.
 */
typedef struct
{
    uint8_t bits;
    uint16_t waiters;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} tiny_events_t;

#endif