/*
    Copyright 2021-2024 (C) Alexey Dynda

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

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include "structmember.h"
//#include "proto/hdlc/low_level/hdlc.h"
#include "proto/fd/tiny_fd.h"
#include "fd.h"

typedef struct
{
    PyObject_HEAD // no semicolon
        tiny_fd_handle_t handle;
    hdlc_crc_t crc_type;
    int mtu;
    int window_size;
    int mode;
    int peers_count;
    int addr;
    void *buffer;
    PyObject *on_frame_send;
    PyObject *on_frame_read;
    PyObject *on_frame_read_ui;
    PyObject *on_connect_event;
    PyObject *read_func;
    PyObject *write_func;
    int error_flag;
} Fd;

static PyMemberDef Fd_members[] = {
    {"mtu", T_INT, offsetof(Fd, mtu), 0, "Maximum size of payload"},
    {"window_size", T_INT, offsetof(Fd, window_size), 0, "Sliding window size (1-7)"},
    {NULL} /* Sentinel */
};

/////////////////////////////// ALLOC/DEALLOC

static void Fd_dealloc(Fd *self)
{
    Py_XDECREF(self->on_frame_read);
    Py_XDECREF(self->on_frame_read_ui);
    Py_XDECREF(self->on_frame_send);
    Py_XDECREF(self->on_connect_event);
    Py_XDECREF(self->read_func);
    Py_XDECREF(self->write_func);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int Fd_init(Fd *self, PyObject *args, PyObject *kwds)
{
    self->handle = NULL;
    self->buffer = NULL;
    self->crc_type = HDLC_CRC_16;
    self->on_frame_send = NULL;
    self->on_frame_read = NULL;
    self->on_frame_read_ui = NULL;
    self->on_connect_event = NULL;
    self->read_func = NULL;
    self->write_func = NULL;
    self->mtu = 1500;
    self->window_size = 7;
    self->mode = TINY_FD_MODE_ABM;
    self->peers_count = 0;
    self->addr = 0;
    self->error_flag = 0;
    return 0;
}

static PyObject *Fd_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Fd *self;

    self = (Fd *)type->tp_alloc(type, 0);
    if ( self != NULL )
    {
        Fd_init(self, args, kwds);
    }
    return (PyObject *)self;
}

////////////////////////////// Internal callbacks

static void on_frame_read(void *user_data, uint8_t addr, uint8_t *data, int len)
{
    Fd *self = (Fd *)user_data;
    if ( self->on_frame_read )
    {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();

        PyObject *arg_addr = PyLong_FromLong((long)addr);
        PyObject *arg = PyByteArray_FromStringAndSize((const char *)data, (Py_ssize_t)len);
        PyObject *temp = PyObject_CallFunctionObjArgs(self->on_frame_read, arg_addr, arg, NULL);
        Py_XDECREF(temp);
        Py_DECREF(arg);
        Py_DECREF(arg_addr);

        PyGILState_Release(gstate);
    }
}

static void on_frame_read_ui(void *user_data, uint8_t addr, uint8_t *data, int len)
{
    Fd *self = (Fd *)user_data;
    if ( self->on_frame_read_ui )
    {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();

        PyObject *arg_addr = PyLong_FromLong((long)addr);
        PyObject *arg = PyByteArray_FromStringAndSize((const char *)data, (Py_ssize_t)len);
        PyObject *temp = PyObject_CallFunctionObjArgs(self->on_frame_read_ui, arg_addr, arg, NULL);
        Py_XDECREF(temp);
        Py_DECREF(arg);
        Py_DECREF(arg_addr);

        PyGILState_Release(gstate);
    }
}

static void on_frame_send(void *user_data, uint8_t addr, const uint8_t *data, int len)
{
    Fd *self = (Fd *)user_data;
    if ( self->on_frame_send )
    {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();

        PyObject *arg_addr = PyLong_FromLong((long)addr);
        PyObject *arg = PyByteArray_FromStringAndSize((const char *)data, (Py_ssize_t)len);
        PyObject *temp = PyObject_CallFunctionObjArgs(self->on_frame_send, arg_addr, arg, NULL);
        Py_XDECREF(temp);
        Py_DECREF(arg);
        Py_DECREF(arg_addr);

        PyGILState_Release(gstate);
    }
}

static void on_connect_event(void *user_data, uint8_t address, bool connected)
{
    Fd *self = (Fd *)user_data;
    if ( self->on_connect_event )
    {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();

        PyObject *arg_address = PyLong_FromLong((long)address);
        PyObject *arg_connected = PyBool_FromLong((long)connected);
        PyObject *temp = PyObject_CallFunctionObjArgs(self->on_connect_event, arg_address, arg_connected, NULL);
        // Dereference result
        Py_XDECREF(temp);
        // Dereference argss
        Py_DECREF(arg_address);
        Py_DECREF(arg_connected);

        PyGILState_Release(gstate);
    }
}

////////////////////////////// METHODS

static PyObject *Fd_begin(Fd *self)
{
    tiny_fd_init_t init{};
    init.pdata = self;
    init.on_read_cb = on_frame_read;
    init.on_send_cb = on_frame_send;
    init.on_connect_event_cb = on_connect_event;
    init.on_read_ui_cb = on_frame_read_ui;
    init.crc_type = self->crc_type;
    init.buffer_size = tiny_fd_buffer_size_by_mtu_ex(
        self->peers_count > 0 ? self->peers_count : 1,
        self->mtu, self->window_size, init.crc_type, 2);
    self->buffer = PyObject_Malloc(init.buffer_size);
    init.buffer = self->buffer;
    init.send_timeout = 1000;
    init.retry_timeout = 200;
    init.retries = 2;
    init.window_frames = self->window_size;
    init.mtu = self->mtu;
    init.mode = (uint8_t)self->mode;
    init.peers_count = (uint8_t)self->peers_count;
    init.addr = (uint8_t)self->addr;
    int result = tiny_fd_init(&self->handle, &init);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_end(Fd *self)
{
    tiny_fd_close(self->handle);
    self->handle = NULL;
    PyObject_Free(self->buffer);
    self->buffer = NULL;
    Py_RETURN_NONE;
}

static PyObject *Fd_disconnect(Fd *self)
{
    int result = 0;
    Py_BEGIN_ALLOW_THREADS;
    result = tiny_fd_disconnect(self->handle);
    Py_END_ALLOW_THREADS;
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_send(Fd *self, PyObject *args)
{
    Py_buffer buffer{};
    if ( !PyArg_ParseTuple(args, "s*", &buffer) )
    {
        return NULL;
    }
    int result;

    Py_BEGIN_ALLOW_THREADS;
    result = tiny_fd_send_packet(self->handle, buffer.buf, buffer.len, 1000);
    Py_END_ALLOW_THREADS;

    PyBuffer_Release(&buffer);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_rx(Fd *self, PyObject *args)
{
    Py_buffer buffer{};
    if ( !PyArg_ParseTuple(args, "s*", &buffer) )
    {
        return NULL;
    }
    int result = tiny_fd_on_rx_data(self->handle, buffer.buf, buffer.len);
    PyBuffer_Release(&buffer);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_tx(Fd *self, PyObject *args)
{
    int result;
    Py_buffer buffer{};
    if ( !PyArg_ParseTuple(args, "|s*", &buffer) )
    {
        return NULL;
    }
    if ( buffer.buf == NULL )
    {
        void *data = PyObject_Malloc(self->mtu);
        result = tiny_fd_get_tx_data(self->handle, data, self->mtu, 0);
        PyObject *to_send = PyByteArray_FromStringAndSize((const char *)data, result);
        PyObject_Free(data);
        return to_send;
    }
    else
    {
        result = tiny_fd_get_tx_data(self->handle, buffer.buf, buffer.len, 0);
        PyBuffer_Release(&buffer);
        return PyLong_FromLong((long)result);
    }
}

int write_func(void *user_data, const void *buffer, int size)
{
    int result = 0;
    Fd *self = (Fd *)user_data;
    if ( self->write_func )
    {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();

        PyObject *arg = PyByteArray_FromStringAndSize((const char *)buffer, (Py_ssize_t)size);
        PyObject *temp = PyObject_CallFunctionObjArgs(self->write_func, arg, NULL);
        if ( !temp || !PyLong_Check(temp) )
        {
            Py_XDECREF(temp); // Dereference result
            Py_DECREF(arg);   // We do not need ByteArray anymore
            self->error_flag = 1;

            PyGILState_Release(gstate);
            return size;
        }
        result = PyLong_AsLong(temp);
        Py_XDECREF(temp); // Dereference result
        Py_DECREF(arg);   // We do not need ByteArray anymore

        PyGILState_Release(gstate);
    }

    return result;
}

static PyObject *Fd_run_tx(Fd *self, PyObject *args)
{
    int result = 0;
    PyObject *cb = NULL;
    if ( !PyArg_ParseTuple(args, "O", &cb) )
    {
        return NULL;
    }
    if ( !PyFunction_Check(cb) )
    {
        return NULL;
    }
    Py_INCREF(cb);
    self->write_func = cb;

    Py_BEGIN_ALLOW_THREADS;
    result = tiny_fd_run_tx(self->handle, write_func);
    Py_END_ALLOW_THREADS;

    Py_DECREF(cb);
    self->write_func = NULL;
    if ( self->error_flag )
    {
        return PyErr_Format(PyExc_RuntimeError, "Write function must return integer number of bytes written");
    }
    return PyLong_FromLong((long)result);
}

int read_func(void *user_data, void *buffer, int size)
{
    int result = 0;
    Fd *self = (Fd *)user_data;
    if ( self->read_func )
    {
        PyGILState_STATE gstate;
        gstate = PyGILState_Ensure();

        PyObject *arg = PyLong_FromLong((long)size);
        PyObject *temp = PyObject_CallFunctionObjArgs(self->read_func, arg, NULL);
        if ( !temp || !PyObject_CheckBuffer(temp) )
        {
            Py_XDECREF(temp); // Dereference result
            Py_DECREF(arg);   // We do not need ByteArray anymore
            self->error_flag = 1;

            PyGILState_Release(gstate);
            return 0;
        }
        Py_buffer view;
        if ( PyObject_GetBuffer(temp, &view, 0) >= 0 )
        {
            memcpy(buffer, view.buf, view.len);
            result = view.len;
            PyBuffer_Release(&view);
        }
        Py_XDECREF(temp); // Dereference result
        Py_DECREF(arg);

        PyGILState_Release(gstate);
    }

    return result;
}

static PyObject *Fd_run_rx(Fd *self, PyObject *args)
{
    int result = 0;
    PyObject *cb = NULL;
    if ( !PyArg_ParseTuple(args, "O", &cb) )
    {
        return NULL;
    }
    if ( !PyFunction_Check(cb) )
    {
        return NULL;
    }
    Py_INCREF(cb);
    self->read_func = cb;

    Py_BEGIN_ALLOW_THREADS;
    result = tiny_fd_run_rx(self->handle, read_func);
    Py_END_ALLOW_THREADS;

    Py_DECREF(cb);
    self->read_func = NULL;
    if ( self->error_flag )
    {
        return PyErr_Format(PyExc_RuntimeError, "Read function must return bytearray with the bytes");
    }
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_get_status(Fd *self)
{
    int result = tiny_fd_get_status(self->handle);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_send_to(Fd *self, PyObject *args)
{
    int addr;
    Py_buffer buffer{};
    if ( !PyArg_ParseTuple(args, "is*", &addr, &buffer) )
    {
        return NULL;
    }
    int result;
    Py_BEGIN_ALLOW_THREADS;
    result = tiny_fd_send_packet_to(self->handle, (uint8_t)addr, buffer.buf, buffer.len, 1000);
    Py_END_ALLOW_THREADS;
    PyBuffer_Release(&buffer);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_register_peer(Fd *self, PyObject *args)
{
    int addr;
    if ( !PyArg_ParseTuple(args, "i", &addr) )
    {
        return NULL;
    }
    int result = tiny_fd_register_peer(self->handle, (uint8_t)addr);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_send_ui(Fd *self, PyObject *args)
{
    Py_buffer buffer{};
    if ( !PyArg_ParseTuple(args, "s*", &buffer) )
    {
        return NULL;
    }
    int result = tiny_fd_send_ui_packet(self->handle, buffer.buf, buffer.len);
    PyBuffer_Release(&buffer);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_send_ui_to(Fd *self, PyObject *args)
{
    int addr;
    Py_buffer buffer{};
    if ( !PyArg_ParseTuple(args, "is*", &addr, &buffer) )
    {
        return NULL;
    }
    int result = tiny_fd_send_ui_packet_to(self->handle, (uint8_t)addr, buffer.buf, buffer.len);
    PyBuffer_Release(&buffer);
    return PyLong_FromLong((long)result);
}

static PyObject *Fd_set_ka_timeout(Fd *self, PyObject *args)
{
    unsigned int timeout;
    if ( !PyArg_ParseTuple(args, "I", &timeout) )
    {
        return NULL;
    }
    tiny_fd_set_ka_timeout(self->handle, (uint32_t)timeout);
    Py_RETURN_NONE;
}

/*
void tiny_fd_set_ka_timeout 	( 	tiny_fd_handle_t  	handle,
                uint32_t  	keep_alive
        )

*/

///////////////////////////////// GETTERS SETTERS

static PyObject *Fd_get_on_read(Fd *self, void *closure)
{
    Py_INCREF(self->on_frame_read);
    return self->on_frame_read;
}

static int Fd_set_on_read(Fd *self, PyObject *value, void *closure)
{
    PyObject *tmp = self->on_frame_read;
    Py_INCREF(value);
    self->on_frame_read = value;
    Py_XDECREF(tmp);
    return 0;
}

static PyObject *Fd_get_on_send(Fd *self, void *closure)
{
    Py_INCREF(self->on_frame_send);
    return self->on_frame_send;
}

static int Fd_set_on_send(Fd *self, PyObject *value, void *closure)
{
    PyObject *tmp = self->on_frame_send;
    Py_INCREF(value);
    self->on_frame_send = value;
    Py_XDECREF(tmp);
    return 0;
}

static PyObject *Fd_get_on_connect_event(Fd *self, void *closure)
{
    Py_INCREF(self->on_connect_event);
    return self->on_connect_event;
}

static int Fd_set_on_connect_event(Fd *self, PyObject *value, void *closure)
{
    PyObject *tmp = self->on_connect_event;
    Py_INCREF(value);
    self->on_connect_event = value;
    Py_XDECREF(tmp);
    return 0;
}

static PyObject *Fd_get_on_read_ui(Fd *self, void *closure)
{
    Py_INCREF(self->on_frame_read_ui);
    return self->on_frame_read_ui;
}

static int Fd_set_on_read_ui(Fd *self, PyObject *value, void *closure)
{
    PyObject *tmp = self->on_frame_read_ui;
    Py_INCREF(value);
    self->on_frame_read_ui = value;
    Py_XDECREF(tmp);
    return 0;
}

static PyObject* Fd_get_crc(Fd *self, void * closure)
{
    return PyLong_FromLong( self->crc_type );
}

static int Fd_set_crc(Fd *self, PyObject *value)
{
    int result = -1;
    if ( value && PyLong_Check(value) )
    {
        int temp = PyLong_AsLong(value);
        if ( temp == HDLC_CRC_16 || temp == HDLC_CRC_32 ||
             temp == HDLC_CRC_8 || temp == HDLC_CRC_OFF ||
             temp == HDLC_CRC_DEFAULT )
        {
             result = 0;
             self->crc_type = (hdlc_crc_t)temp;
        }
    }
    if ( result < 0 )
    {
        PyErr_Format(PyExc_RuntimeError, "Allowable CRC values are: 0 (AUTO), 8, 16, 32, 255 (OFF)");
    }
    return result;
}

static PyObject *Fd_get_mode(Fd *self, void *closure)
{
    return PyLong_FromLong(self->mode);
}

static int Fd_set_mode(Fd *self, PyObject *value, void *closure)
{
    if ( value && PyLong_Check(value) )
    {
        int temp = PyLong_AsLong(value);
        if ( temp == TINY_FD_MODE_ABM || temp == TINY_FD_MODE_NRM )
        {
            self->mode = temp;
            return 0;
        }
    }
    PyErr_Format(PyExc_RuntimeError, "Allowable mode values are: 0 (ABM), 1 (NRM)");
    return -1;
}

static PyObject *Fd_get_peers_count(Fd *self, void *closure)
{
    return PyLong_FromLong(self->peers_count);
}

static int Fd_set_peers_count(Fd *self, PyObject *value, void *closure)
{
    if ( value && PyLong_Check(value) )
    {
        int temp = PyLong_AsLong(value);
        if ( temp >= 0 && temp <= 63 )
        {
            self->peers_count = temp;
            return 0;
        }
    }
    PyErr_Format(PyExc_RuntimeError, "peers_count must be 0-63");
    return -1;
}

static PyObject *Fd_get_addr(Fd *self, void *closure)
{
    return PyLong_FromLong(self->addr);
}

static int Fd_set_addr(Fd *self, PyObject *value, void *closure)
{
    if ( value && PyLong_Check(value) )
    {
        int temp = PyLong_AsLong(value);
        if ( temp >= 0 && temp <= 62 )
        {
            self->addr = temp;
            return 0;
        }
    }
    PyErr_Format(PyExc_RuntimeError, "addr must be 0 (primary) or 1-62 (secondary)");
    return -1;
}

static PyGetSetDef Fd_getsetters[] = {
    {"on_read", (getter)Fd_get_on_read, (setter)Fd_set_on_read, "Callback for incoming I-frames: func(addr, data)", NULL},
    {"on_read_ui", (getter)Fd_get_on_read_ui, (setter)Fd_set_on_read_ui, "Callback for incoming UI frames: func(addr, data)", NULL},
    {"on_send", (getter)Fd_get_on_send, (setter)Fd_set_on_send, "Callback for successfully sent messages: func(addr, data)", NULL},
    {"on_connect_event", (getter)Fd_get_on_connect_event, (setter)Fd_set_on_connect_event,
     "Callback for connection status change: func(addr, connected)", NULL},
    {"crc", (getter)Fd_get_crc, (setter)Fd_set_crc, "CRC type: 0 (AUTO), 8, 16, 32, 255 (OFF)", NULL},
    {"mode", (getter)Fd_get_mode, (setter)Fd_set_mode, "Protocol mode: 0 (ABM), 1 (NRM)", NULL},
    {"peers_count", (getter)Fd_get_peers_count, (setter)Fd_set_peers_count, "Max peers (0-63, NRM primary only)", NULL},
    {"addr", (getter)Fd_get_addr, (setter)Fd_set_addr, "Station address: 0 (primary) or 1-62 (secondary)", NULL},
    {NULL} /* Sentinel */
};

///////////////////////////////// BINDINGS

static PyMethodDef Fd_methods[] = {
    {"begin", (PyCFunction)Fd_begin, METH_NOARGS, "Initializes Fd protocol"},
    {"end", (PyCFunction)Fd_end, METH_NOARGS, "Stops Fd protocol"},
    {"send", (PyCFunction)Fd_send, METH_VARARGS, "Sends new message to remote side"},
    {"send_to", (PyCFunction)Fd_send_to, METH_VARARGS, "Sends message to specific peer: send_to(addr, data)"},
    {"send_ui", (PyCFunction)Fd_send_ui, METH_VARARGS, "Sends UI (connectionless) frame"},
    {"send_ui_to", (PyCFunction)Fd_send_ui_to, METH_VARARGS, "Sends UI frame to specific peer: send_ui_to(addr, data)"},
    {"register_peer", (PyCFunction)Fd_register_peer, METH_VARARGS, "Registers peer address (NRM primary)"},
    {"disconnect", (PyCFunction)Fd_disconnect, METH_NOARGS, "Sends disconnect frame"},
    {"rx", (PyCFunction)Fd_rx, METH_VARARGS, "Passes rx data"},
    {"tx", (PyCFunction)Fd_tx, METH_VARARGS, "Fills specified buffer with tx data"},
    {"run_rx", (PyCFunction)Fd_run_rx, METH_VARARGS, "Reads data from user callback and parses them"},
    {"run_tx", (PyCFunction)Fd_run_tx, METH_VARARGS, "Writes data to user callback"},
    {"get_status", (PyCFunction)Fd_get_status, METH_NOARGS, "Get connection status"},
    {"set_ka_timeout", (PyCFunction)Fd_set_ka_timeout, METH_VARARGS, "Set keep-alive timeout in ms"},
    {NULL} /* Sentinel */
};

PyTypeObject FdType = {
    PyVarObject_HEAD_INIT(NULL, 0) "tinyproto.Fd", /* tp_name */
    sizeof(Fd),                                    /* tp_basicsize */
    0,                                             /* tp_itemsize */
    (destructor)Fd_dealloc,                        /* tp_dealloc */
    0,                                             /* tp_print */
    0,                                             /* tp_getattr */
    0,                                             /* tp_setattr */
    0,                                             /* tp_reserved */
    0,                                             /* tp_repr */
    0,                                             /* tp_as_number */
    0,                                             /* tp_as_sequence */
    0,                                             /* tp_as_mapping */
    0,                                             /* tp_hash  */
    0,                                             /* tp_call */
    0,                                             /* tp_str */
    0,                                             /* tp_getattro */
    0,                                             /* tp_setattro */
    0,                                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,      /* tp_flags */
    "Fd object",                                   /* tp_doc */
    0,                                             /* tp_traverse */
    0,                                             /* tp_clear */
    0,                                             /* tp_richcompare */
    0,                                             /* tp_weaklistoffset */
    0,                                             /* tp_iter */
    0,                                             /* tp_iternext */
    Fd_methods,                                    /* tp_methods */
    Fd_members,                                    /* tp_members */
    Fd_getsetters,                                 /* tp_getset */
    0,                                             /* tp_base */
    0,                                             /* tp_dict */
    0,                                             /* tp_descr_get */
    0,                                             /* tp_descr_set */
    0,                                             /* tp_dictoffset */
    (initproc)Fd_init,                             /* tp_init */
    0,                                             /* tp_alloc */
    Fd_new,                                        /* tp_new */
};
