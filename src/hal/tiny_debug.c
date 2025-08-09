/*
    Copyright 2025 (C) Alexey Dynda

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

#include "tiny_debug.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>


#if defined(TINY_DEBUG) && defined(TINY_FILE_LOGGING)

static FILE *log_file = NULL;

static FILE *get_log_file(uintptr_t id)
{
    if (log_file == NULL) {
        char filename[64];
        snprintf(filename, sizeof(filename), "tiny_fd_%08" PRIxPTR ".csv", id);
        log_file = fopen(filename, "w");
        if (log_file == NULL) {
            fprintf(stderr, "Failed to open log file\n");
            return NULL;
        }
        // Num# - is frame number, Exp# - is expected frame number from remote peer
        fprintf(log_file, " time ms, DIR, ADDR, FR, Type, Num#, Exp#\n");
    }
    return log_file;
}

void tiny_file_log(uintptr_t id, const char *fmt, ...)
{
    FILE *file = get_log_file(id);
    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fflush(file);
}

#endif