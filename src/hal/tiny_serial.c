/*
    Copyright 2017-2025 (C) Alexey Dynda

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

#include "tiny_serial.h"

#if defined(ARDUINO) || defined(__AVR__)

#elif defined(__linux__)

#include "linux/linux_serial.inl"

#elif defined(__APPLE__) && defined(__MACH__)

#include "macos/macos_serial.inl"

#elif defined(_WIN32)

#include "win32/win32_serial.inl"

#elif defined(__XTENSA__)

#include "esp32/esp32_serial.inl"

#else

#include "no_platform/noplatform_serial.inl"

#endif
