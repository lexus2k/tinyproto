/*
    Copyright 2019-2024 (C) Alexey Dynda

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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <inttypes.h>

#ifndef TINY_LOG_LEVEL_DEFAULT
#define TINY_LOG_LEVEL_DEFAULT 0
#endif

#ifdef TINY_ABORT_ON_ERROR
#include <stdlib.h>
#define TINY_ABORT() abort()
#else
#define TINY_ABORT()
#endif

    extern uint8_t g_tiny_log_level;

    enum
    {
        TINY_LOG_CRIT = 0,
        TINY_LOG_ERR = 1,
        TINY_LOG_WRN = 2,
        TINY_LOG_INFO = 3,
        TINY_LOG_DEB = 4,
    };

#ifndef TINY_DEBUG
#define TINY_DEBUG 1
#endif

//extern int my_printf( const char *str, ... );

#if TINY_DEBUG
#define TINY_LOG(lvl, fmt, ...)                                                                                        \
    {                                                                                                                  \
        if ( lvl < g_tiny_log_level )                                                                                  \
            fprintf(stderr, "%08" PRIu32 " ms: " fmt, tiny_millis(), ##__VA_ARGS__);                                   \
    }
#define TINY_LOG0(lvl, fmt)                                                                                            \
    {                                                                                                                  \
        if ( lvl < g_tiny_log_level )                                                                                  \
            fprintf(stderr, "%08" PRIu32 " ms: " fmt, tiny_millis());                                                  \
    }
#ifdef TINY_FILE_LOGGING
void tiny_file_log(uintptr_t id, const char *fmt, ...);
#define TINY_FILE_LOG(id, fmt, ...) \
    { \
        tiny_file_log(id, "%08" PRIu32 ", " fmt, tiny_millis(), ##__VA_ARGS__); \
    }
#else
#define TINY_FILE_LOG(...)
#endif
#else
#define TINY_LOG(...)
#define TINY_LOG0(...)
#define TINY_FILE_LOG(...)
#endif

#ifdef __cplusplus
}
#endif

#endif
