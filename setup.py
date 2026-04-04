"""
    Copyright 2021-2025 (C) Alexey Dynda

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
"""

import glob
from distutils.core import setup, Extension
import platform

source_files =       glob.glob('./src/**/*.c*', recursive=True )
source_files.extend( glob.glob('./python/**/*.cpp', recursive=True ) )

if platform.system() == "Linux":
    libs = ["stdc++"] # Required for Linux and doesn't work in Windows
else:
    libs = []

tinyproto_module = Extension(
    'tinyproto',
    sources=source_files,
    include_dirs=['./src','./python'],
    define_macros=[("MYDEF", None), ("TINY_FD_DEBUG", 0), ("TINY_LOG_LEVEL_DEFAULT", 0)],
    library_dirs=[],
    libraries=libs,
    language='c++',
)


# It has also scripts argument to build
setup(
    name='tinyproto',
    author='Alexey Dynda',
    author_email='alexey.dynda@gmail.com',
    url='https://github.com/lexus2k/tinyproto',
    license='LGPLv3',
    version='1.2.0',
    description='tinyproto module wrapper',
    package_dir = { "tinyproto": "./python" },
    packages = ['tinyproto.wrappers', 'tinyproto.helpers'],
    ext_modules=[tinyproto_module],
)

