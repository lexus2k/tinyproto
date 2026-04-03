/*
    Copyright 2019-2025 (C) Alexey Dynda

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

/**
 This is Tiny Full-Duplex protocol implementation for microcontrollers.
 It is built on top of Tiny Protocol (hdlc/low_level/hdlc.c)

 @file
 @brief Tiny Protocol Full Duplex API

 @details Implements full duplex asynchronous ballanced mode (ABM)
*/
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "proto/crc/tiny_crc.h"
#include "hal/tiny_types.h"

    /**
     * @defgroup FULL_DUPLEX_API Tiny Full Duplex API functions
     * @{
     */

    /**
     * Address of primary stations to use with the protocol. See RFC.
     */
    #define TINY_FD_PRIMARY_ADDR (0)

    enum
    {
        /**
         * Asynchronous balanced mode - the default mode of HDLC protocol.
         * ABM adds combined terminal which can act as both a primary and a secondary.
         */
        TINY_FD_MODE_ABM = 0x00,

        /**
         * Normal response mode allows the secondary-to-primary link to be shared without
         * contention, because it has the primary give the secondaries permission to transmit
         * one at a time.
         * @warning This mode is still in development
         */
        TINY_FD_MODE_NRM = 0x01,

        /**
         * Asynchronous response mode. NOT IMPLEMENTED.
         */
        TINY_FD_MODE_ARM = 0x02,
    };

    /** 
     * Forward declaration of tiny_fd_data_t structure.
     */
    struct tiny_fd_data_t;

    /**
     * This handle points to service data, required for full-duplex
     * functioning.
     */
    typedef struct tiny_fd_data_t *tiny_fd_handle_t;

    /**
     * Enumeration of frame types used in Tiny Full Duplex protocol.
     */
    typedef enum
    {
        TINY_FD_FRAME_TYPE_I = 0x00, ///< I-frame
        TINY_FD_FRAME_TYPE_S = 0x01, ///< S-frame
        TINY_FD_FRAME_TYPE_U = 0x02, ///< U-frame
    } tiny_fd_frame_type_t;

    /**
     * Enumeration of frame subtypes used in Tiny Full Duplex protocol.
     * These subtypes are used for S-frames and U-frames.
     * The I-frames do not have subtypes, so this enumeration is not used for I-frames.
     */
    typedef enum
    {
        TINY_FD_FRAME_SUBTYPE_RR = 0x00, ///< S-frame subtype RR
        TINY_FD_FRAME_SUBTYPE_RNR = 0x04, ///< S-frame subtype RNR
        TINY_FD_FRAME_SUBTYPE_REJ = 0x08, ///< S-frame subtype REJ

        TINY_FD_FRAME_SUBTYPE_UA = 0x60, ///< U-frame subtype UA
        TINY_FD_FRAME_SUBTYPE_DM = 0x0C, ///< U-frame subtype DM
        TINY_FD_FRAME_SUBTYPE_FRMR = 0x84, ///< U-frame subtype FRMR
        TINY_FD_FRAME_SUBTYPE_RSET = 0x8C, ///< U-frame subtype RSET
        TINY_FD_FRAME_SUBTYPE_SABM = 0x2C, ///< U-frame subtype SABM
        TINY_FD_FRAME_SUBTYPE_SNRM = 0x80, ///< U-frame subtype SNRM
        TINY_FD_FRAME_SUBTYPE_DISC = 0x40, ///< U-frame subtype DISC
    } tiny_fd_frame_subtype_t;

    /**
     * Enumeration of frame directions used in Tiny Full Duplex protocol.
     */
    typedef enum
    {
        TINY_FD_FRAME_DIRECTION_IN = 0x00, ///< Frame is incoming
        TINY_FD_FRAME_DIRECTION_OUT = 0x01, ///< Frame is outgoing
    } tiny_fd_frame_direction_t;

    /**
     * tiny_fd_log_frame_cb_t is a callback function, which is called every time frame is sent or received.
     * This callback is useful for debugging purposes.
     * 
     * The main difference between this callback and on_frame_read_cb_t / on_frame_send_cb_t is that
     * this callback is called for every frame, regardless of whether it is sent or received: I-frames, S-frames or U-frames.
     * It provides detailed information about the frame, including its type, subtype, sequence numbers and data.
     * 
     * @param udata user data, passed during Tiny Full Duplex initialization.
     * @param handle handle of Tiny.
     * @param direction direction of the frame, can be TINY_FD_FRAME_DIRECTION_IN or TINY_FD_FRAME_DIRECTION_OUT.
     * @param frame_type type of the frame, can be TINY_FD_FRAME_TYPE_I, TINY_FD_FRAME_TYPE_S or TINY_FD_FRAME_TYPE_U.
     * @param frame_subtype subtype of the frame, can be TINY_FD_FRAME_SUBTYPE_RR, TINY_FD_FRAME_SUBTYPE_REJ,
     *                     TINY_FD_FRAME_SUBTYPE_UA, TINY_FD_FRAME_SUBTYPE_FRMR, TINY_FD_FRAME_SUBTYPE_RSET,
     *                     TINY_FD_FRAME_SUBTYPE_SABM, TINY_FD_FRAME_SUBTYPE_SNRM or TINY_FD_FRAME_SUBTYPE_DISC.
     * @param ns N(S) sequence number of the frame.
     * @param nr N(R) sequence number of the frame.
     * @param data pointer to the frame data.
     * @param len length of the frame data.
     */
    typedef void (*tiny_fd_log_frame_cb_t)(void *udata,
                       tiny_fd_handle_t handle,
                       tiny_fd_frame_direction_t direction,
                       tiny_fd_frame_type_t frame_type,
                       tiny_fd_frame_subtype_t frame_subtype,
                       uint8_t ns,
                       uint8_t nr,
                       const uint8_t *data,
                       int len);
   
    /**
     * This structure is used for initialization of Tiny Full Duplex protocol.
     */
    typedef struct tiny_fd_init_t_
    {
        /// user data for block read/write functions
        void *pdata;

        /// callback function to process incoming frames. Callback is called from tiny_fd_run_rx() context.
        on_frame_read_cb_t on_read_cb;

        /// Callback to get notification of sent frames. Callback is called from tiny_fd_run_tx() context.
        on_frame_send_cb_t on_send_cb;

        /**
         * buffer to store data during full-duplex protocol operating.
         * The size should be at least size returned by tiny_fd_buffer_size_by_mtu()
         */
        void *buffer;

        /// maximum input buffer size, see tiny_fd_buffer_size_by_mtu()
        uint16_t buffer_size;

        /**
         * timeout. Can be set to 0 during initialization. In this case timeout will be set to default.
         * Timeout parameter sets timeout in milliseconds for blocking API functions: tiny_fd_send().
         */
        uint16_t send_timeout;

        /**
         * timeout for retry operation. It is valid and applicable to I-frames only.
         * retry_timeout sets timeout in milliseconds. If zero value is specified, it is calculated as
         */
        uint16_t retry_timeout;

        /**
         * number retries to perform before timeout takes place
         */
        uint8_t retries;

        /**
         * crc field type to use on hdlc level.
         * If HDLC_CRC_DEFAULT is passed, crc type will be selected automatically (depending on library configuration),
         * but HDLC_CRC_16 has higher priority.
         */
        hdlc_crc_t crc_type;

        /**
         * Number of frames in window, which confirmation may be deferred for. Must be at least 1. Maximum allowable
         * value is 7. Extended HDLC format (with 127 window size) is not yet supported.
         * Smaller values reduce channel throughput, while higher values require more RAM.
         * It is not mandatory to have the same window_frames value on both endpoints.
         */
        uint8_t window_frames;

        /**
         * Maximum transmission unit in bytes. If this parameter is zero, the protocol
         * will automatically calculate mtu based on buffer_size, window_frames.
         */
        int mtu;

        /**
         * Callback to get the notification when connect or disconnect event takes place.
         * Can be NULL.
         */
        on_connect_event_cb_t on_connect_event_cb;

        /**
         * Callback to log frames. Can be NULL.
         * If this callback is set, it will be called for every frame sent or received.
         * This is useful for debugging purposes.
         */
        tiny_fd_log_frame_cb_t log_frame_cb;

        /**
         * Local station address. The field has meaning only for secondary stations.
         * For primary stations please, leave this field as 0.
         * For secondary stations the allowable range is 1 - 62.
         * All 0-address is used only for testing purposes and is not allowed.
         * All 1-address is used as broadcast address and tinyproto will filter out it.
         */
        uint8_t addr;

        /**
         * Maximum number of peers supported by the local station.
         * If the value is equal to 0, that means that only one remote station is supported.
         * For secondary stations this value must be set to 1 or 0.
         * For primary stations this value can be in range 0 - 63.
         * @warning Use 1 or 0 for now
         */
        uint8_t peers_count;

        /**
         * Communication link mode. Refer to TINY_FD_MODE_ABM, TINY_FD_MODE_NRM, TINY_FD_MODE_ARM.
         */
        uint8_t mode;

    } tiny_fd_init_t;

    /**
     * @brief Initialized communication for Tiny Full Duplex protocol.
     *
     * The function initializes internal structures for Tiny Full Duplex state machine.
     *
     * @param handle - pointer to Tiny Full Duplex data
     * @param init - pointer to tiny_fd_init_t data.
     * @return TINY_NO_ERROR in case of success.
     *         TINY_ERR_FAILED if init parameters are incorrect.
     * @remarks This function is not thread safe.
     */
    extern int tiny_fd_init(tiny_fd_handle_t *handle, tiny_fd_init_t *init);

    /**
     * @brief Returns status of the connection
     *
     * The function returns status of the connection
     *
     * @param handle pointer to Tiny Full Duplex data
     * @return TINY_ERR_INVALID_DATA in case of error
     *         TINY_SUCCESS if connection is established
     *         TINY_ERR_FAILED if protocol is in disconnected state
     */
    extern int tiny_fd_get_status(tiny_fd_handle_t handle);

    /**
     * @brief Sends DISC command to remote side
     *
     * The function sends DISC command to remote side and exits. It doesn't wait for UA answer back.
     *
     * @param handle pointer to Tiny Full Duplex data
     * @return TINY_ERR_INVALID_DATA in case of error
     *         TINY_SUCCESS if DISC command is put to tx queue successfully
     *         TINY_ERR_FAILED if DISC command cannot be put to tx queue since it is full.
     */
    extern int tiny_fd_disconnect(tiny_fd_handle_t handle);

    /**
     * @brief stops Tiny Full Duplex state machine
     *
     * stops Tiny Full Duplex state machine.
     *
     * @param handle handle of full-duplex protocol
     */
    extern void tiny_fd_close(tiny_fd_handle_t handle);

    /**
     * @brief runs tx processing to fill specified buffer with data.
     *
     * Runs tx processing to fill specified buffer with data.
     *
     * @param handle handle of full-duplex protocol
     * @param data pointer to buffer to fill with tx data
     * @param len maximum size of specified buffer
     * @param timeout in milliseconds to wait for the data to be ready for sending
     * @return number of bytes written to specified buffer
     */
    extern int tiny_fd_get_tx_data(tiny_fd_handle_t handle, void *data, int len, uint32_t timeout);

    /**
     * @brief sends tx data to the communication channel via user callback `write_func()`.
     *
     * Sends tx data to the communication channel via user callback `write_func()`.
     * Internally this function generates next 4 bytes to send (or less if nothing to send),
     * and calls user callback write_func() until all generated bytes are sent, or error
     * happens. This function helps to simplify application code.
     *
     * @param handle handle of full-duplex protocol
     * @param write_func callback to the function to write data to the physical channel.
     *
     * @return number of bytes sent
     */
    extern int tiny_fd_run_tx(tiny_fd_handle_t handle, write_block_cb_t write_func);

    /**
     * @brief runs rx bytes processing for specified buffer.
     *
     * Runs rx bytes processing for specified buffer.
     * This is alternative method to run Full-duplex protocol for
     * rx data. Use it, when you wish to control reading from hardware
     * by yourself.
     *
     * @param handle handle of full-duplex protocol
     * @param data pointer to data to process
     * @param len length of data to process
     * @return TINY_SUCCESS
     */
    extern int tiny_fd_on_rx_data(tiny_fd_handle_t handle, const void *data, int len);

    /**
     * @brief reads rx data from the communication channel via user callback `read_func()`
     *
     * Reads rx data from the communication channel via user callback `read_func()`.
     * Internally this function has 4-byte buffer, and tries to read 4 bytes from the channel.
     * Then received bytes are processed by the protocol. If FD protocol detects new incoming
     * message then it calls on_send_callback.
     * If no data available in the channel, the function returns immediately after read_func() callback
     * returns control.
     *
     * @param handle handle of full-duplex protocol
     * @param read_func callback to the function to read data from the physical channel.
     * @return number of bytes received
     */
    extern int tiny_fd_run_rx(tiny_fd_handle_t handle, read_block_cb_t read_func);

    /**
     * @brief Sends userdata over full-duplex protocol.
     *
     * Sends userdata over full-duplex protocol. Note, that this command
     * will return success, when data are copied to internal queue. That doesn't mean
     * that data are physically sent, but they are enqueued for sending.
     *
     * @note the maximum size of the allowed buffer to send is equal to mtu size set for
     *       the protocol. If you want to send data larger than mtu size, please, use
     *       tiny_fd_send() function.
     *
     * When timeout happens, the data were not actually enqueued. Call this function once again.
     * If TINY_ERR_DATA_TOO_LARGE is returned, try to send less data. If you don't want to care about
     * mtu size, please, use different function tiny_fd_send()..
     *
     * @param handle   tiny_fd_handle_t handle
     * @param address  address of remote peer. For primary device, please use TINY_FD_PRIMARY_ADDR
     * @param buf      data to send
     * @param len      length of data to send
     * @param timeout  timeout in milliseconds to wait until data are placed to outgoing queue
     *
     * @return Success result or error code:
     *         * TINY_SUCCESS          if user data are put to internal queue.
     *         * TINY_ERR_TIMEOUT      if no room in internal queue to put data. Retry operation once again.
     *         * TINY_ERR_FAILED       if request was cancelled, by tiny_fd_close() or other error happened.
     *         * TINY_ERR_UNKNOWN_PEER if peer is not known to the system.
     *         * TINY_ERR_DATA_TOO_LARGE if user data are too big to fit in tx buffer.
     */
    extern int tiny_fd_send_packet_to(tiny_fd_handle_t handle, uint8_t address, const void *buf, int len, uint32_t timeout);

    /**
     * Returns minimum required buffer size for specified parameters.
     *
     * @note This function calculates buffer requirements based on HDLC_CRC_16.
     *
     * @param mtu size of desired user payload in bytes.
     * @param window maximum tx queue size of I-frames.
     */
    extern int tiny_fd_buffer_size_by_mtu(int mtu, int window);

    /**
     * Returns minimum required buffer size for specified parameters.
     *
     * @param peers_count maximum number of peers supported by the primary. Use 0 or 1 for secondary devices
     * @param mtu size of desired user payload in bytes.
     * @param tx_window maximum tx queue size of I-frames.
     * @param crc_type crc type to be used with FD protocol
     * @param rx_window number of RX ring buffer in frames
     */
    extern int tiny_fd_buffer_size_by_mtu_ex(uint8_t peers_count, int mtu, int tx_window, hdlc_crc_t crc_type, int rx_window);

    /**
     * @brief returns max packet size in bytes.
     *
     * Returns max packet size in bytes.
     *
     * @param handle   tiny_fd_handle_t handle
     * @return mtu size in bytes
     * @see tiny_fd_send_packet
     */
    extern int tiny_fd_get_mtu(tiny_fd_handle_t handle);

    /**
     * @brief Sends userdata over full-duplex protocol.
     *
     * Sends userdata over full-duplex protocol. Note, that this function will try to send
     * all data from buf. In success case it will return number of bytes sent, equal to len
     * input parameter. But if timeout happens, it returns number of bytes actually enqueued.
     *
     * If you constantly get number of sent bytes less than expected, try to increase
     * timeout value of the speed of used communication channel.
     *
     * @param handle   tiny_fd_handle_t handle
     * @param address  address of remote peer. For primary device, please use TINY_FD_PRIMARY_ADDR
     * @param buf      data to send
     * @param len      length of data to send
     * @param timeout  timeout in milliseconds, will be used for each block sending
     *
     * @return Number of bytes sent
     */
    extern int tiny_fd_send_to(tiny_fd_handle_t handle, uint8_t address, const void *buf, int len, uint32_t timeout);

    /**
     * Sets keep alive timeout in milliseconds. This timeout is used to send special RR
     * frames, when no user data queued for sending.
     * @param handle   pointer to tiny_fd_handle_t
     * @param keep_alive timeout in milliseconds
     */
    extern void tiny_fd_set_ka_timeout(tiny_fd_handle_t handle, uint32_t keep_alive);

    /**
     * Registers remote peer with specified address. This API can be used only in NRM mode
     * on primary station. The allowable range of the addresses is 1 - 62.
     * The addresses 0, 63 cannot be used as they are dedicated to primary station and legacy support.
     * After secondary station is registered, the primary will send establish connection to remote station.
     * If remote station is not yet ready, this can cause reducing of the primary station performance.
     *
     * @param handle   pointer to tiny_fd_handle_t
     * @param address  address in range 1 - 62.
     *
     * @return TINY_SUCCESS in case of success or TINY_ERR_FAILED if there is no free slots
     *         for new peer, wrong address is specified, or peer is already registered.
     */
    extern int tiny_fd_register_peer(tiny_fd_handle_t handle, uint8_t address);

    /**
     * @brief Sends userdata over full-duplex protocol to primary station.
     *
     * Sends userdata over full-duplex protocol to primary station. For details, please, refer to tiny_fd_send_to().
     *
     * @param handle   tiny_fd_handle_t handle
     * @param buf      data to send
     * @param len      length of data to send
     * @param timeout  timeout in milliseconds, will be used for each block sending
     *
     * @return Number of bytes sent
     */
    extern int tiny_fd_send(tiny_fd_handle_t handle, const void *buf, int len, uint32_t timeout);

    /**
     * Sends packet to primary station. For details, please, refer to tiny_fd_send_packet_to().
     *
     * @param handle   tiny_fd_handle_t handle
     * @param buf      data to send
     * @param len      length of data to send
     * @param timeout  timeout in milliseconds to wait until data are placed to outgoing queue
     *
     * @return Success result or error code
     * @note This function doesn't return number of bytes sent, but rather returns
     *       TINY_SUCCESS in case of success, or error code in case of failure.
     */
    extern int tiny_fd_send_packet(tiny_fd_handle_t handle, const void *buf, int len, uint32_t timeout);

    /**
     * @}
     */

#ifdef __cplusplus
}
#endif
