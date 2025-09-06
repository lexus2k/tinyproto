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

#pragma once

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifdef __cplusplus
extern "C"
{
#endif

#include "tiny_fd_frames_int.h"
#include "hal/tiny_types.h"
#include <stdint.h>
#include <stdbool.h>

    typedef struct
    {
        tiny_fd_frame_info_t **frames;  ///< pointer to the frame table
        int size;                       ///< number of elements in the table
        int lookup_index;               ///< First index to start search from
        int mtu;                        ///< Maximum supported payload size
    } tiny_fd_queue_t;

    typedef struct
    {
        /// Storage for all I- frames
        tiny_fd_queue_t i_queue;
        /// Storage for all S- and U- service frames
        tiny_fd_queue_t s_queue;
        /// Global mutex
        tiny_mutex_t mutex;

    } tiny_frames_queue_t;

    /**
     * Initializes the queue, and returns number of bytes allocated in the provided buffer
     * In case of error returns negative values (error codes)
     *
     * @param queue pointer to queue structure
     * @param buffer buffer to store queue data
     * @param max_size maximum size of the provided buffer
     * @param max_frames maximum number of frames to store
     * @param mtu maximum size of user payload
     */
    int tiny_fd_queue_init(tiny_fd_queue_t *queue, uint8_t *buffer,
                           int max_size, int max_frames, int mtu);

    /**
     * Resets the queue to its default state, flushes all stored frames
     */
    void tiny_fd_queue_reset(tiny_fd_queue_t *queue);

    /**
     * Reset the queue only for specific address
     */
    void tiny_fd_queue_reset_for(tiny_fd_queue_t *queue, uint8_t address);

    /**
     * Returns true if the queue has free slots
     */
    bool tiny_fd_queue_has_free_slots(tiny_fd_queue_t *queue);

    /**
     * Returns max payload size, supported for the queued frames
     */
    int tiny_fd_queue_get_mtu(tiny_fd_queue_t *queue);

    /**
     * Allocates free slot in the queue and copies user data to the queue.
     * If there are no space returns NULL, otherwise returns pointer to allocated frame info structure.
     */
    tiny_fd_frame_info_t *tiny_fd_queue_allocate(tiny_fd_queue_t *queue, uint8_t type, const uint8_t *data, int len);

    /**
     * Returns pointer to the next element with speciifed type and arg or NULL.
     *
     * @param queue pointer to queue structure
     * @param type type of the record to search for: tiny_fd_queue_type_t
     * @param arg arg used as the address field only for I-frame and must contain frame number to search for.
     *
     * @important Remember that S-Frames and U-Frames can be reordered by the queue.
     */
    tiny_fd_frame_info_t *tiny_fd_queue_get_next(tiny_fd_queue_t *queue, uint8_t type, uint8_t address, uint8_t arg);

    /**
     * Marks frame slot as free
     *
     * @param queue pointer to queue structure
     * @param frame pointer to the frame information
     */
    void tiny_fd_queue_free(tiny_fd_queue_t *queue, tiny_fd_frame_info_t *frame);

    /**
     * Marks frame slot as free, using pointer to header
     *
     * @param queue pointer to queue structure
     * @param frame pointer to the frame information
     */
    void tiny_fd_queue_free_by_header(tiny_fd_queue_t *queue, const void *header);

#ifdef __cplusplus
}
#endif

#endif
