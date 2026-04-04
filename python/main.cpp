/*
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
*/

#include <Python.h>
#include "hdlc_ll.h"
#include "fd.h"
#include "py_serial.h"
#include "proto/fd/tiny_fd.h"
#include "proto/crc/tiny_crc.h"

static PyMethodDef tinyproto_methods[] = {
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef tinyproto_definition = {PyModuleDef_HEAD_INIT, "tinyproto", "A Python tiny protocol module",
                                                   -1, tinyproto_methods};

PyMODINIT_FUNC PyInit_tinyproto(void)
{
    Py_Initialize();
    PyObject *m = PyModule_Create(&tinyproto_definition);

    if ( PyType_Ready(&HdlcType) < 0 )
    {
        return NULL;
    }

    if ( PyType_Ready(&FdType) < 0 )
    {
        return NULL;
    }

    if ( PyType_Ready(&SerialType) < 0 )
    {
        return NULL;
    }

    Py_INCREF(&HdlcType);
    PyModule_AddObject(m, "Hdlc", (PyObject *)&HdlcType);
    Py_INCREF(&FdType);
    PyModule_AddObject(m, "Fd", (PyObject *)&FdType);
    Py_INCREF(&SerialType);
    PyModule_AddObject(m, "Serial", (PyObject *)&SerialType);

    // Protocol mode constants
    PyModule_AddIntConstant(m, "MODE_ABM", TINY_FD_MODE_ABM);
    PyModule_AddIntConstant(m, "MODE_NRM", TINY_FD_MODE_NRM);
    PyModule_AddIntConstant(m, "PRIMARY_ADDR", TINY_FD_PRIMARY_ADDR);

    // CRC type constants
    PyModule_AddIntConstant(m, "CRC_DEFAULT", HDLC_CRC_DEFAULT);
    PyModule_AddIntConstant(m, "CRC_8", HDLC_CRC_8);
    PyModule_AddIntConstant(m, "CRC_16", HDLC_CRC_16);
    PyModule_AddIntConstant(m, "CRC_32", HDLC_CRC_32);
    PyModule_AddIntConstant(m, "CRC_OFF", HDLC_CRC_OFF);

    return m;
}
