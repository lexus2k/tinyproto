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

#include "tiny_types.h"
#include "tiny_debug.h"

#if defined(CONFIG_ENABLE_CPP_HAL)
// Do not include anything here, there is tiny_types_cpp.cpp for it
#elif defined(TINY_CUSTOM_PLATFORM)
#include "no_platform/no_platform_hal.inl"
#elif defined(__AVR__)
#include "avr/avr_hal.inl"
#elif defined(__XTENSA__)
#include "esp32/esp32_hal.inl"
#elif defined(ARDUINO)
#include "arduino/arduino_hal.inl"
#elif defined(__linux__)
#include "linux/linux_hal.inl"
#elif defined(__APPLE__) && defined(__MACH__)
#include "macos/macos_hal.inl"
#elif defined(__MINGW32__)
#include "mingw32/mingw32_hal.inl"
#elif defined(_WIN32)
#include "win32/win32_hal.inl"
#elif defined(CPU_S32K144HFT0VLLT)
#include "freertos/freertos_hal.inl"
#else
#warning "Platform not supported. Multithread support is disabled"
#include "no_platform/no_platform_hal.inl"
#endif

uint8_t g_tiny_log_level = TINY_LOG_LEVEL_DEFAULT;

void tiny_log_level(uint8_t level)
{
    g_tiny_log_level = level;
}
