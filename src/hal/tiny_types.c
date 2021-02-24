/*
    Copyright 2016-2019 (C) Alexey Dynda

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

#include "tiny_types.h"

#if defined(__AVR__)
#include "impl/avr_hal.inl"
#elif defined(__XTENSA__)
#include "impl/esp32_hal.inl"
#elif defined(ARDUINO)
#include "impl/arduino_hal.inl"
#elif defined(__linux__)
#include "impl/linux_hal.inl"
#elif defined(__MINGW32__)
#include "impl/mingw32_hal.inl"
#elif defined(_WIN32)
#include "impl/win32_hal.inl"
#else
#warning "Platform not supported. Multithread support is disabled"
#include "impl/no_platform_hal.inl"
#endif

#include "tiny_debug.h"

uint8_t g_tiny_log_level = TINY_LOG_LEVEL_DEFAULT;

void tiny_log_level(uint8_t level)
{
    g_tiny_log_level = level;
}
