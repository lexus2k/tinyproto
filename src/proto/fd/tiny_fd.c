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

#include "tiny_fd.h"
#include "tiny_fd_int.h"
#include "tiny_fd_defines_int.h"
#include "tiny_fd_peers_int.h"
#include "tiny_fd_service_queue_int.h"
#include "tiny_fd_data_queue_int.h"
#include "tiny_fd_on_rx_int.h"
#include "hal/tiny_types.h"
#include "hal/tiny_debug.h"

#include <string.h>


static void on_frame_read(void *user_data, uint8_t *data, int len);
static void on_frame_send(void *user_data, const uint8_t *data, int len);

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////

static uint8_t __switch_to_next_peer(tiny_fd_handle_t handle)
{
    const uint8_t start_peer = handle->next_peer;
    do
    {
        if ( ++handle->next_peer >= handle->peers_count )
        {
            handle->next_peer = 0;
        }
        if ( handle->peers[ handle->next_peer ].addr != 0xFF )
        {
            break;
        }
    } while ( start_peer != handle->next_peer );
    LOG(TINY_LOG_INFO, "[%p] Switching to peer [%02X]\n", handle, handle->next_peer);
    return start_peer != handle->next_peer;
}

///////////////////////////////////////////////////////////////////////////////

#if 0
static inline uint8_t __number_of_awaiting_tx_i_frames(tiny_fd_handle_t handle, uint8_t peer)
{
    return ((uint8_t)(handle->peers[peer].last_ns - handle->peers[peer].confirm_ns) & seq_bits_mask);
}
#endif

///////////////////////////////////////////////////////////////////////////////

static inline uint32_t __time_passed_since_last_frame_received(tiny_fd_handle_t handle, uint8_t peer)
{
    return (uint32_t)(tiny_millis() - handle->peers[peer].last_ka_ts);
}

///////////////////////////////////////////////////////////////////////////////

static inline uint32_t __time_passed_since_last_marker_seen(tiny_fd_handle_t handle)
{
    return (uint32_t)(tiny_millis() - handle->last_marker_ts);
}

///////////////////////////////////////////////////////////////////////////////

static void __switch_to_connected_state(tiny_fd_handle_t handle, uint8_t peer)
{
    if ( handle->peers[peer].state != TINY_FD_STATE_CONNECTED )
    {
        handle->peers[peer].state = TINY_FD_STATE_CONNECTED;
        handle->peers[peer].confirm_ns = 0;
        handle->peers[peer].last_ns = 0;
        handle->peers[peer].next_ns = 0;
        handle->peers[peer].next_nr = 0;
        handle->peers[peer].sent_nr = 0;
        handle->peers[peer].sent_reject = 0;
        tiny_fd_queue_reset_for( &handle->frames.i_queue, __peer_to_address_field( handle, peer ) );
        // Reset last arrived frame timestamp on connection.
        // This is required to avoid disconnection on keep alive timeout at the beginning of connection
        handle->peers[peer].last_ka_ts = tiny_millis();
        tiny_events_set(&handle->peers[peer].events, FD_EVENT_CAN_ACCEPT_I_FRAMES);
        tiny_events_set(
            &handle->events,
            FD_EVENT_TX_DATA_AVAILABLE |
                (tiny_fd_queue_has_free_slots(&handle->frames.i_queue) ? FD_EVENT_QUEUE_HAS_FREE_SLOTS : 0));
        LOG(TINY_LOG_CRIT, "[%p] Connection is established\n", handle);
        if ( handle->on_connect_event_cb )
        {
            tiny_mutex_unlock(&handle->frames.mutex);
            handle->on_connect_event_cb(handle->user_data,
                                       __is_primary_station( handle ) ? (__peer_to_address_field( handle, peer ) >> 2) : TINY_FD_PRIMARY_ADDR,
                                       true);
            tiny_mutex_lock(&handle->frames.mutex);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

static void __switch_to_disconnected_state(tiny_fd_handle_t handle, uint8_t peer)
{
    if ( handle->peers[peer].state != TINY_FD_STATE_DISCONNECTED )
    {
        handle->peers[peer].state = TINY_FD_STATE_DISCONNECTED;
        handle->peers[peer].confirm_ns = 0;
        handle->peers[peer].last_ns = 0;
        handle->peers[peer].next_ns = 0;
        handle->peers[peer].next_nr = 0;
        handle->peers[peer].sent_nr = 0;
        handle->peers[peer].sent_reject = 0;
        tiny_fd_queue_reset_for( &handle->frames.i_queue, __peer_to_address_field( handle, peer ) );
        tiny_events_clear(&handle->peers[peer].events, FD_EVENT_CAN_ACCEPT_I_FRAMES);
        LOG(TINY_LOG_CRIT, "[%p] Disconnected\n", handle);
        if ( handle->on_connect_event_cb )
        {
            tiny_mutex_unlock(&handle->frames.mutex);
            handle->on_connect_event_cb(handle->user_data,
                                       __is_primary_station( handle ) ? (__peer_to_address_field( handle, peer ) >> 2) : TINY_FD_PRIMARY_ADDR,
                                        false);
            tiny_mutex_lock(&handle->frames.mutex);
        }
    }
}


///////////////////////////////////////////////////////////////////////////////

static void on_frame_read(void *user_data, uint8_t *data, int len)
{
    tiny_fd_handle_t handle = (tiny_fd_handle_t)user_data;
    if ( len < 2 )
    {
        LOG(TINY_LOG_WRN, "%s: received too small frame\n", "FD");
        return;
    }
    uint8_t peer = __address_field_to_peer( handle, ((uint8_t *)data)[0] );
    if ( peer == 0xFF )
    {
        // it seems that the frame is not for us. Just exit
        return;
    }
    tiny_mutex_lock(&handle->frames.mutex);
    handle->peers[peer].last_ka_ts = tiny_millis();
    handle->peers[peer].ka_confirmed = 1;
    uint8_t control = ((uint8_t *)data)[1];
    if ( (control & HDLC_U_FRAME_MASK) == HDLC_U_FRAME_MASK )
    {
        __on_u_frame_read(handle, peer, data, len);
    }
    else if ( handle->peers[peer].state != TINY_FD_STATE_CONNECTED && handle->peers[peer].state != TINY_FD_STATE_DISCONNECTING )
    {
        // Should send DM in case we receive here S- or I-frames.
        // If connection is not established, we should ignore all frames except U-frames
        LOG(TINY_LOG_CRIT, "[%p] Connection is not established, connecting\n", handle);
        tiny_frame_header_t frame = {
            .address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
            .control = (handle->mode == TINY_FD_MODE_NRM ? HDLC_U_FRAME_TYPE_SNRM : HDLC_U_FRAME_TYPE_SABM) | HDLC_U_FRAME_BITS,
        };
        __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2);
        FILE_LOG((uintptr_t)handle, "OUT", 'U',
            (handle->mode == TINY_FD_MODE_NRM ? "SNRM" : "SABM"), 0, 0);
        handle->peers[peer].state = TINY_FD_STATE_CONNECTING;
    }
    else if ( (control & HDLC_I_FRAME_MASK) == HDLC_I_FRAME_BITS )
    {
        __on_i_frame_read(handle, peer, data, len);
    }
    else if ( (control & HDLC_S_FRAME_MASK) == HDLC_S_FRAME_BITS )
    {
        __on_s_frame_read(handle, peer, data, len);
    }
    else
    {
        LOG(TINY_LOG_WRN, "[%p] Unknown hdlc frame received\n", handle);
    }
    if ( control & HDLC_P_BIT )
    {
        // Check that if we are in NRM mode then we have something to send
        if ( handle->mode == TINY_FD_MODE_NRM )
        {
            LOG(TINY_LOG_INFO, "[%p] [CAPTURED MARKER]\n", handle);
        }
        // Cool! Now we have marker again, and we can send
        tiny_events_set( &handle->events, FD_EVENT_HAS_MARKER );
    }
    tiny_mutex_unlock(&handle->frames.mutex);
}

static void on_frame_send(void *user_data, const uint8_t *data, int len)
{
    tiny_fd_handle_t handle = (tiny_fd_handle_t)user_data;
    uint8_t peer = __address_field_to_peer( handle, ((const uint8_t *)data)[0] );
    uint8_t control = ((uint8_t *)data)[1];
    if ( peer == 0xFF )
    {
        // Do nothing for now, but this should never happen
        return;
    }
    tiny_mutex_lock(&handle->frames.mutex);
    if ( (control & HDLC_I_FRAME_MASK) == HDLC_I_FRAME_BITS )
    {
        (void)(peer);
        // nothing to do
        // we need to wait for confirmation from remote side
    }
    else if ( (control & HDLC_S_FRAME_MASK) == HDLC_S_FRAME_BITS )
    {
        tiny_fd_queue_free_by_header( &handle->frames.s_queue, data );
    }
    else if ( (control & HDLC_U_FRAME_MASK) == HDLC_U_FRAME_BITS )
    {
        tiny_fd_queue_free_by_header( &handle->frames.s_queue, data );
    }
    // Clear send flag and clear marker if final was transferred. For ABM mode the marker is never cleared
    uint8_t flags = FD_EVENT_TX_SENDING;
    if ( (control & HDLC_F_BIT) && handle->mode == TINY_FD_MODE_NRM )
    {
        // Let's talk to the next station if we are primary
        // Of course, we could switch to the next peer upon receving response
        // from the peer we provided the marker to... But what? What if
        // remote peer never responds to us. So, having switch procedure
        // in this callback simplifies things
        if ( __is_primary_station( handle ) )
        {
            __switch_to_next_peer( handle );
        }
        flags |= FD_EVENT_HAS_MARKER;
        LOG(TINY_LOG_INFO, "[%p] [RELEASED MARKER]\n", handle);
    }
    tiny_events_clear( &handle->events, flags );
    tiny_mutex_unlock(&handle->frames.mutex);
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_init(tiny_fd_handle_t *handle, tiny_fd_init_t *init)
{
    const uint8_t peers_count = init->peers_count == 0 ? 1 : init->peers_count;
    *handle = NULL;
    if ( (0 == init->on_read_cb) || (0 == init->buffer) || (0 == init->buffer_size) )
    {
        LOG(TINY_LOG_CRIT, "Invalid input data: null pointers%s", "\n");
        TINY_ABORT();
        return TINY_ERR_INVALID_DATA;
    }
    if ( init->mtu == 0 )
    {
        int size = tiny_fd_buffer_size_by_mtu_ex(peers_count, 0, init->window_frames, init->crc_type, 1);
        init->mtu = (init->buffer_size - size) / (init->window_frames + 1);
        if ( init->mtu < 1 )
        {
            LOG(TINY_LOG_CRIT, "Calculated mtu size is zero, no payload transfer is available%s", "\n");
            TINY_ABORT();
            return TINY_ERR_OUT_OF_MEMORY;
        }
    }
    if ( init->buffer_size < tiny_fd_buffer_size_by_mtu_ex(peers_count, init->mtu, init->window_frames, init->crc_type, 1) )
    {
        LOG(TINY_LOG_CRIT, "Too small buffer for FD protocol %i < %i\n", init->buffer_size,
            tiny_fd_buffer_size_by_mtu_ex(peers_count, init->mtu, init->window_frames, init->crc_type, 1));
        TINY_ABORT();
        return TINY_ERR_OUT_OF_MEMORY;
    }
    if ( init->window_frames < 2 )
    {
        LOG(TINY_LOG_CRIT, "HDLC doesn't support less than 2-frames queue%s", "\n");
        TINY_ABORT();
        return TINY_ERR_INVALID_DATA;
    }
    if ( !init->retry_timeout && !init->send_timeout )
    {
        LOG(TINY_LOG_CRIT, "HDLC uses timeouts for ACK, at least retry_timeout, or send_timeout must be specified%s", "\n");
        TINY_ABORT();
        return TINY_ERR_INVALID_DATA;
    }
    memset(init->buffer, 0, init->buffer_size);

    /* Lets locate main FD protocol data at the beginning of specified buffer.
     * The buffer must be properly aligned for ARM processors to get correct alignment for tiny_fd_data_t structure.
     * That's why we allocate the space for the tiny_fd_data_t structure at the beginning. */
    uint8_t *ptr = TINY_ALIGN_BUFFER(init->buffer);
    tiny_fd_data_t *protocol = (tiny_fd_data_t *)ptr;
    ptr += sizeof(tiny_fd_data_t);
    /* Next let's allocate the space for low level hdlc structure. It will be located right next to the tiny_fd_data_t.
     * To do that we need to calculate the size required for all FD buffers
     * We do not need to align the buffer for the HDLC level, since it done by low level API. */
    uint8_t *hdlc_ll_ptr = ptr;
    // TODO: Hack: remove - 4
    int hdlc_ll_size = (int)((uint8_t *)init->buffer + init->buffer_size - ptr - 4 - // Remaining size
                             init->window_frames *                               // Number of frames multiply by frame size (headers + payload + pointers)
                                 ( sizeof(tiny_fd_frame_info_t *) + init->mtu + sizeof(tiny_fd_frame_info_t) - sizeof(((tiny_fd_frame_info_t *)0)->payload) ) -
                             TINY_FD_U_QUEUE_MAX_SIZE *
                                 (sizeof(tiny_fd_frame_info_t *) + sizeof(tiny_fd_frame_info_t)) -
                             peers_count * sizeof(tiny_fd_peer_info_t));
    /* All FD protocol structures must be aligned. */
    hdlc_ll_size &= ~(TINY_ALIGN_STRUCT_VALUE - 1);
    ptr += hdlc_ll_size;
    ptr = TINY_ALIGN_BUFFER(ptr);

    /* Next we need some space to hold pointers to tiny_i_frame_info_t records (window_frames pointers) */
    int queue_size = tiny_fd_queue_init( &protocol->frames.i_queue, ptr, (int)((uint8_t *)init->buffer + init->buffer_size - ptr),
                                         init->window_frames, init->mtu );
    if ( queue_size < 0 )
    {
        return queue_size;
    }
    ptr += queue_size;
    ptr = TINY_ALIGN_BUFFER(ptr);
    queue_size = tiny_fd_queue_init( &protocol->frames.s_queue, ptr, (int)((uint8_t *)init->buffer + init->buffer_size - ptr),
                                     TINY_FD_U_QUEUE_MAX_SIZE, 2 );
    if ( queue_size < 0 )
    {
        return queue_size;
    }
    ptr += queue_size;

    /* Next we allocate some space for peer-related data */
    ptr = TINY_ALIGN_BUFFER(ptr);
    protocol->peers_count = peers_count;
    protocol->peers = (tiny_fd_peer_info_t *)ptr;
    protocol->next_peer = 0;
    ptr += sizeof(tiny_fd_peer_info_t) * peers_count;

    if ( ptr > (uint8_t *)init->buffer + init->buffer_size )
    {
        LOG(TINY_LOG_CRIT, "Out of provided memory: provided %i bytes, used %i bytes\n", init->buffer_size,
            (int)(ptr - (uint8_t *)init->buffer));
        return TINY_ERR_OUT_OF_MEMORY;
    }
    /* Lets initialize memory for HDLC low level protocol */
    hdlc_ll_init_t _init = { 0 };
    _init.on_frame_read = on_frame_read;
    _init.on_frame_send = on_frame_send;
    _init.user_data = protocol;
    _init.crc_type = init->crc_type;
    _init.buf_size = hdlc_ll_size;
    _init.buf = hdlc_ll_ptr;
    _init.mtu = init->mtu + sizeof(tiny_frame_header_t);

    int result = hdlc_ll_init(&protocol->_hdlc, &_init);
    if ( result != TINY_SUCCESS )
    {
        LOG(TINY_LOG_CRIT, "HDLC low level initialization failed%s", "\n");
        return result;
    }

    protocol->user_data = init->pdata;
    protocol->on_read_cb = init->on_read_cb;
    protocol->on_send_cb = init->on_send_cb;
    protocol->on_connect_event_cb = init->on_connect_event_cb;
    protocol->send_timeout = init->send_timeout;
    // By default assign primary address
    protocol->addr = (init->addr ? (init->addr << 2) : HDLC_PRIMARY_ADDR ) | HDLC_E_BIT;
    protocol->mode = init->mode;
    // Primary devices always have markers
    protocol->ka_timeout = 5000;
    protocol->retry_timeout = init->retry_timeout ? init->retry_timeout : (protocol->send_timeout / (init->retries + 1));
    protocol->retries = init->retries;
    for (uint8_t peer = 0; peer < protocol->peers_count; peer++ )
    {
        protocol->peers[peer].retries = init->retries;
        // Initialize all remotes addresses
        if ( __is_secondary_station( protocol ) || protocol->mode == TINY_FD_MODE_ABM )
        {
            // Secondary station always responds with its own address.
            // In SABM mode all stations are primaries and send data with the same address
            protocol->peers[peer].addr = protocol->addr;
        }
        else
        {
            protocol->peers[peer].addr = 0xFF;
        }
        protocol->peers[peer].state = TINY_FD_STATE_DISCONNECTED;
        tiny_events_create(&protocol->peers[peer].events);
    }

    tiny_mutex_create(&protocol->frames.mutex);
    tiny_events_create(&protocol->events);
    tiny_events_set( &protocol->events, FD_EVENT_QUEUE_HAS_FREE_SLOTS |
                                        (__is_primary_station( protocol ) ? FD_EVENT_HAS_MARKER : 0) );
    *handle = protocol;

    return TINY_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

void tiny_fd_close(tiny_fd_handle_t handle)
{
    hdlc_ll_close(handle->_hdlc);
    for (uint8_t peer = 0; peer < handle->peers_count; peer++ )
    {
        tiny_events_destroy(&handle->peers[peer].events);
    }
    tiny_events_destroy(&handle->events);
    tiny_mutex_destroy(&handle->frames.mutex);
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_on_rx_data(tiny_fd_handle_t handle, const void *data, int len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    while ( len )
    {
        int error;
        int processed_bytes = hdlc_ll_run_rx(handle->_hdlc, ptr, len, &error);
        if ( error == TINY_ERR_WRONG_CRC )
        {
            LOG(TINY_LOG_WRN, "[%p] HDLC CRC sum mismatch\n", handle);
        }
        ptr += processed_bytes;
        len -= processed_bytes;
    }
    return TINY_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_run_rx(tiny_fd_handle_t handle, read_block_cb_t read_func)
{
    uint8_t buf[4];
    int len = read_func(handle->user_data, buf, sizeof(buf));
    if ( len <= 0 )
    {
        return len;
    }
    return tiny_fd_on_rx_data(handle, buf, len);
}

///////////////////////////////////////////////////////////////////////////////

static uint8_t *tiny_fd_get_next_i_frame(tiny_fd_handle_t handle, int *len, uint8_t peer, uint8_t address)
{
    uint8_t *data = NULL;
    tiny_fd_frame_info_t *ptr = NULL;
    if ( handle->peers[peer].state == TINY_FD_STATE_DISCONNECTED || handle->peers[peer].state == TINY_FD_STATE_CONNECTING )
    {
        // If sending of I-frames is not allowed then just exit
        return NULL;
    }
    ptr = tiny_fd_queue_get_next( &handle->frames.i_queue, TINY_FD_QUEUE_I_FRAME, address, handle->peers[peer].next_ns );
    if ( ptr != NULL )
    {
        data = (uint8_t *)&ptr->header;
        *len = ptr->len + sizeof(tiny_frame_header_t);
        LOG(TINY_LOG_INFO, "[%p] Sending I-Frame N(R-awaiting)=%02X,N(S-seq sent)=%02X with address [%02X] to %s\n", handle, handle->peers[peer].next_nr,
            handle->peers[peer].next_ns, data[0], __is_primary_station( handle ) ? "secondary" : "primary" );
        ptr->header.control &= 0x0F;
        ptr->header.control |= (handle->peers[peer].next_nr << 5);
        handle->peers[peer].next_ns++;
        handle->peers[peer].next_ns &= seq_bits_mask;
        // Move to different place
        handle->peers[peer].sent_nr = handle->peers[peer].next_nr;
        handle->peers[peer].last_i_ts = tiny_millis();
    }
    return data;
}

///////////////////////////////////////////////////////////////////////////////

static uint8_t *tiny_fd_get_next_frame_to_send(tiny_fd_handle_t handle, int *len, uint8_t peer)
{
    uint8_t *data = NULL;
    tiny_mutex_lock(&handle->frames.mutex);
    const uint8_t address = __peer_to_address_field( handle, peer );
    data = tiny_fd_get_next_s_u_frame_to_send(handle, len, peer, address);
    if ( data == NULL )
    {
        data = tiny_fd_get_next_i_frame(handle, len, peer, address);
    }
    if ( data == NULL && handle->mode == TINY_FD_MODE_NRM )
    {
        LOG(TINY_LOG_INFO, "[%p] NOTHING TO SEND TO %s ??? \n", handle, __is_primary_station( handle ) ? "secondary" : "primary");
        // Nothing to send, just send anything to the peer station to pass the marker
        if ( __is_primary_station( handle ) &&
            ( handle->peers[peer].state == TINY_FD_STATE_DISCONNECTED || handle->peers[peer].state == TINY_FD_STATE_CONNECTING))
        {
            tiny_frame_header_t frame = {
                .address = address,
                .control = HDLC_U_FRAME_TYPE_SNRM | HDLC_U_FRAME_BITS,
            };
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, 2);
            FILE_LOG((uintptr_t)handle, "OUT", 'U', "SNRM", 0, 0);
        }
        else
        {
            tiny_frame_header_t frame = {
                .address = address,
                .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RR | (handle->peers[peer].next_nr << 5),
            };
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, 2);
            FILE_LOG((uintptr_t)handle, "OUT", 'S', "  RR", 0, handle->peers[peer].next_nr);
        }
        data = tiny_fd_get_next_s_u_frame_to_send(handle, len, peer, address);
    }
    if ( data != NULL )
    {
        tiny_frame_header_t *header = (tiny_frame_header_t *)data;
        header->control |= HDLC_P_BIT;
        handle->last_marker_ts = tiny_millis();
        handle->peers[peer].last_ka_ts = tiny_millis();
    }
    tiny_mutex_unlock(&handle->frames.mutex);
    return data;
}

///////////////////////////////////////////////////////////////////////////////

static void tiny_fd_connected_check_idle_timeout(tiny_fd_handle_t handle, uint8_t peer)
{
    tiny_mutex_lock(&handle->frames.mutex);
    // If all I-frames are sent and no respond from the remote side
    if ( __has_unconfirmed_frames(handle, peer) && __all_frames_are_sent(handle, peer) &&
         __time_passed_since_last_i_frame(handle, peer) >= handle->retry_timeout )
    {
        // if sent frame was not confirmed due to noisy line
        if ( handle->peers[peer].retries > 0 )
        {
            LOG(TINY_LOG_WRN,
                "[%p] Timeout, resending unconfirmed frames: last(%" PRIu32 " ms, now(%" PRIu32 " ms), timeout(%" PRIu32
                " ms))\n",
                handle, handle->peers[peer].last_i_ts, tiny_millis(), handle->retry_timeout);
            handle->peers[peer].retries--;
            // Do not use mutex for confirm_ns value as it is byte-value
            __resend_all_unconfirmed_frames(handle, peer, 0, handle->peers[peer].confirm_ns);
        }
        else
        {
            LOG(TINY_LOG_CRIT, "[%p] Remote side not responding, flushing I-frames\n", handle);
            __switch_to_disconnected_state(handle, peer);
        }
    }
    else if ( __time_passed_since_last_frame_received(handle, peer) > handle->ka_timeout )
    {
        if ( !handle->peers[peer].ka_confirmed )
        {
            LOG(TINY_LOG_CRIT, "[%p] No keep alive after timeout\n", handle);
            __switch_to_disconnected_state(handle, peer);
        }
        else
        {
            // Nothing to send, all frames are confirmed, just send keep alive
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ),
                .control = HDLC_S_FRAME_BITS | HDLC_S_FRAME_TYPE_RR | (handle->peers[peer].next_nr << 5),
            };
            handle->peers[peer].ka_confirmed = 0;
            __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_S_FRAME, &frame, 2);
            FILE_LOG((uintptr_t)handle, "OUT", 'S', "  RR", 0, handle->peers[peer].next_nr);
        }
        handle->peers[peer].last_ka_ts = tiny_millis();
    }
    tiny_mutex_unlock(&handle->frames.mutex);
}

///////////////////////////////////////////////////////////////////////////////

static void tiny_fd_disconnected_check_idle_timeout(tiny_fd_handle_t handle, uint8_t peer)
{
    tiny_mutex_lock(&handle->frames.mutex);
    if ( __time_passed_since_last_frame_received(handle, peer) >= handle->retry_timeout )
    {
        if ( __is_primary_station( handle ) ) // Only primary station can request connection
        {
            LOG(TINY_LOG_ERR, "[%p] Connection is not established, connecting to peer %02X [addr:%02X]\n", handle,
                   handle->next_peer, __peer_to_address_field( handle, peer ));
            // Try to establish Connection
            tiny_frame_header_t frame = {
                .address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
                .control = (handle->mode == TINY_FD_MODE_NRM ? HDLC_U_FRAME_TYPE_SNRM : HDLC_U_FRAME_TYPE_SABM) | HDLC_U_FRAME_BITS,
            };
            if ( __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2) == NULL )
            {
                LOG(TINY_LOG_CRIT, "[%p] Failed to queue SNRM/SABM message for peer %02X [addr:%02X]\n", handle,
                       handle->next_peer, __peer_to_address_field( handle, peer ));
            }
            FILE_LOG((uintptr_t)handle, "OUT", 'U',
                (handle->mode == TINY_FD_MODE_NRM ? "SNRM" : "SABM"), 0, 0);
            handle->peers[peer].state = TINY_FD_STATE_CONNECTING;
            handle->peers[peer].last_ka_ts = tiny_millis();
        }
    }
    tiny_mutex_unlock(&handle->frames.mutex);
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_get_tx_data(tiny_fd_handle_t handle, void *data, int len, uint32_t timeout)
{
    bool repeat = true;
    int result = 0;
    // TODO: Check for correct mutex usage here. Some fields are not protected
    const uint8_t peer = handle->next_peer;
    while ( result < len )
    {
        int generated_data = 0;
        // Check if send on hdlc level operation is in progress and do some work
        if ( tiny_events_wait(&handle->events, FD_EVENT_TX_SENDING, EVENT_BITS_LEAVE, 0) )
        {
            generated_data = hdlc_ll_run_tx(handle->_hdlc, ((uint8_t *)data) + result, len - result);
        }
        else
        {
            if ( handle->peers[peer].addr == 0xFF )
            {
                result = TINY_ERR_UNKNOWN_PEER;
                break;
            }
            if ( handle->peers[peer].state == TINY_FD_STATE_CONNECTED || handle->peers[peer].state == TINY_FD_STATE_DISCONNECTING )
            {
                tiny_fd_connected_check_idle_timeout(handle, peer);
            }
            else // TINY_FD_STATE_CONNECTING || TINY_FD_STATE_DISCONNECTED
            {
                tiny_fd_disconnected_check_idle_timeout(handle, peer);
            }
            // Since no send operation is in progress, check if we have something to send
            // Check if the station has marker to send FIRST (That means, we are allowed to send anything still)
            if ( tiny_events_wait(&handle->events, FD_EVENT_HAS_MARKER, EVENT_BITS_LEAVE, timeout ) )
            {
                if ( tiny_events_wait(&handle->events, FD_EVENT_TX_DATA_AVAILABLE, EVENT_BITS_CLEAR, timeout) || handle->mode == TINY_FD_MODE_NRM )
                {
                    int frame_len = 0;
                    uint8_t *frame_data = tiny_fd_get_next_frame_to_send(handle, &frame_len, peer);
                    if ( frame_data != NULL )
                    {
                        // Force to check for new frame once again
                        tiny_events_set(&handle->events, FD_EVENT_TX_DATA_AVAILABLE);
                        tiny_events_set(&handle->events, FD_EVENT_TX_SENDING);
                        // Do not use timeout for hdlc_send(), as hdlc level is ready to accept next frame
                        // (FD_EVENT_TX_SENDING is not set). And at this step we do not need hdlc_send() to
                        // send data.
                        hdlc_ll_put_frame(handle->_hdlc, frame_data, frame_len);
                        continue;
                    }
                    else if ( handle->mode == TINY_FD_MODE_ABM || __is_secondary_station( handle ) )
                    {
                        break;
                    }
                }
                else if ( handle->mode == TINY_FD_MODE_ABM || __is_secondary_station( handle ) )
                {
                    break;
                }
            }
            else if ( __is_primary_station( handle ) )
            {
                if ( __time_passed_since_last_marker_seen(handle) >= handle->retry_timeout )
                {
                    // Return marker back as remote station not responding
                    LOG(TINY_LOG_CRIT, "[%p] RETURN MARKER BACK\n", handle );
                    tiny_events_set( &handle->events, FD_EVENT_HAS_MARKER );
                }
                else
                {
                    break;
                }
            }
        }
        result += generated_data;
        if ( !generated_data )
        {
            if ( !repeat )
            {
                break;
            }
            repeat = false;
        }
        else
        {
            repeat = true;
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_run_tx(tiny_fd_handle_t handle, write_block_cb_t write_func)
{
    uint8_t buf[4];
    int len = tiny_fd_get_tx_data(handle, buf, sizeof(buf), 1);
    if ( len <= 0 )
    {
        return len;
    }
    uint8_t *ptr = buf;
    while ( len )
    {
        int result = write_func(handle->user_data, ptr, len);
        if ( result < 0 )
        {
            return result;
        }
        len -= result;
        ptr += result;
    }
    return TINY_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_send_packet_to(tiny_fd_handle_t handle, uint8_t address, const void *data, int len, uint32_t timeout)
{
    int result = TINY_SUCCESS;
    uint8_t peer;
    LOG(TINY_LOG_DEB, "[%p] PUT frame\n", handle);
    if ( __is_secondary_station( handle ) && address == TINY_FD_PRIMARY_ADDR )
    {
        // For secondary stations the address is actually from field
        address = handle->addr;
    }
    peer = __address_field_to_peer( handle, (address << 2) | HDLC_E_BIT );
    if ( peer == 0xFF )
    {
        LOG(TINY_LOG_ERR, "[%p] PUT frame error: Unknown peer\n", handle);
        TINY_ABORT();
        return TINY_ERR_UNKNOWN_PEER;
    }
    // Check frame size againts mtu
    // MTU doesn't include header and crc fields, only user payload
    uint32_t start_ms = tiny_millis();
    if ( len > tiny_fd_queue_get_mtu( &handle->frames.i_queue ) )
    {
        LOG(TINY_LOG_ERR, "[%p] PUT frame error: data len %i is greater MTU %i\n", handle, len, handle->frames.i_queue.mtu);
        TINY_ABORT();
        result = TINY_ERR_DATA_TOO_LARGE;
    }
    // Wait until there is room for new frame
    else if ( tiny_events_wait(&handle->peers[peer].events, FD_EVENT_CAN_ACCEPT_I_FRAMES, EVENT_BITS_CLEAR, timeout) )
    {
        uint32_t delta_ms = (uint32_t)(tiny_millis() - start_ms);
        if ( tiny_events_wait(&handle->events, FD_EVENT_QUEUE_HAS_FREE_SLOTS, EVENT_BITS_CLEAR,
                               timeout > delta_ms ? (timeout - delta_ms) : 0) )
        {
            tiny_mutex_lock(&handle->frames.mutex);
            // Check if space is actually available
            if ( __put_i_frame_to_tx_queue(handle, peer, data, len) )
            {
                if ( tiny_fd_queue_has_free_slots( &handle->frames.i_queue ) )
                {
                    LOG(TINY_LOG_INFO, "[%p] I_QUEUE is N(S)queue=%d, N(S)confirm=%d, N(S)next=%d\n", handle,
                        handle->peers[peer].last_ns, handle->peers[peer].confirm_ns, handle->peers[peer].next_ns);
                    tiny_events_set(&handle->events, FD_EVENT_QUEUE_HAS_FREE_SLOTS);
                }
                else
                {
                    LOG(TINY_LOG_WRN, "[%p] I_QUEUE is full N(S-free)queue=%d, N(S-awaiting confirm)confirm=%d, N(S-to send)next=%d\n", handle,
                        handle->peers[peer].last_ns, handle->peers[peer].confirm_ns, handle->peers[peer].next_ns);
                }
                result = TINY_SUCCESS;
            }
            else
            {
                result = TINY_ERR_TIMEOUT;
                // !!!! If this log appears, then in the code of the protocol something is definitely wrong !!!!
                LOG(TINY_LOG_ERR, "[%p] Wrong flag FD_EVENT_QUEUE_HAS_FREE_SLOTS\n", handle);
                TINY_ABORT();
            }
            if ( __can_accept_i_frames( handle, peer ) )
            {
                tiny_events_set(&handle->peers[peer].events, FD_EVENT_CAN_ACCEPT_I_FRAMES);
            }
            tiny_mutex_unlock(&handle->frames.mutex);
        }
        else
        {
            // Put flag back, since HDLC protocol allows to send next frame, while
            // Tx queue is completely busy
            tiny_events_set(&handle->peers[peer].events, FD_EVENT_CAN_ACCEPT_I_FRAMES);
            LOG(TINY_LOG_WRN, "[%p] PUT frame timeout\n", handle);
            result = TINY_ERR_TIMEOUT;
        }
    }
    else
    {
        LOG(TINY_LOG_ERR, "[%p] PUT frame timeout\n", handle);
        TINY_ABORT();
        result = TINY_ERR_TIMEOUT;
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_send_packet(tiny_fd_handle_t handle, const void *data, int len, uint32_t timeout)
{
    return tiny_fd_send_packet_to(handle, TINY_FD_PRIMARY_ADDR, data, len, timeout);
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_buffer_size_by_mtu(int mtu, int window)
{
    return tiny_fd_buffer_size_by_mtu_ex(0, mtu, window, HDLC_CRC_16, 1);
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_buffer_size_by_mtu_ex(uint8_t peers_count, int mtu, int tx_window, hdlc_crc_t crc_type, int rx_window)
{
    if ( !peers_count )
    {
        peers_count = 1;
    }
    // Alignment requirements are already satisfied by hdlc_ll_get_buf_size_ex() subfunction call
    return sizeof(tiny_fd_data_t) + TINY_ALIGN_STRUCT_VALUE - 1 +
           peers_count * sizeof(tiny_fd_peer_info_t) +
           // RX side
           hdlc_ll_get_buf_size_ex(mtu + sizeof(tiny_frame_header_t), crc_type, rx_window) +
           // TX side
           (sizeof(tiny_fd_frame_info_t *) + sizeof(tiny_fd_frame_info_t) + mtu -
            sizeof(((tiny_fd_frame_info_t *)0)->payload)) *
               tx_window +
           (sizeof(tiny_fd_frame_info_t *) + sizeof(tiny_fd_frame_info_t)) * TINY_FD_U_QUEUE_MAX_SIZE;
}

///////////////////////////////////////////////////////////////////////////////

void tiny_fd_set_ka_timeout(tiny_fd_handle_t handle, uint32_t keep_alive)
{
    handle->ka_timeout = keep_alive;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_get_mtu(tiny_fd_handle_t handle)
{
    return tiny_fd_queue_get_mtu( &handle->frames.i_queue );
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_send_to(tiny_fd_handle_t handle, uint8_t address, const void *data, int len, uint32_t timeout)
{
    const uint8_t *ptr = (const uint8_t *)data;
    int left = len;
    while ( left > 0 )
    {
        int size = left < tiny_fd_queue_get_mtu( &handle->frames.i_queue ) ? left : tiny_fd_queue_get_mtu( &handle->frames.i_queue );
        int result = tiny_fd_send_packet_to(handle, address, ptr, size, timeout);
        if ( result != TINY_SUCCESS )
        {
            break;
        }
        left -= result;
    }
    return left;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_send(tiny_fd_handle_t handle, const void *data, int len, uint32_t timeout)
{
    return tiny_fd_send_to(handle, (HDLC_PRIMARY_ADDR >> 2), data, len, timeout);
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_get_status(tiny_fd_handle_t handle)
{
    uint8_t peer = 0; // TODO: Request for specific peer
    if ( !handle )
    {
         return TINY_ERR_INVALID_DATA;
    }
    int result = TINY_ERR_FAILED;
    tiny_mutex_lock(&handle->frames.mutex);
    if ( (handle->peers[peer].state == TINY_FD_STATE_CONNECTED) || (handle->peers[peer].state == TINY_FD_STATE_DISCONNECTING) )
    {
        result = TINY_SUCCESS;
    }
    tiny_mutex_unlock(&handle->frames.mutex);
    return result;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_disconnect(tiny_fd_handle_t handle)
{
    uint8_t peer = 0; // TODO: Loop for all peers or for specific peer
    if ( !handle )
    {
         return TINY_ERR_INVALID_DATA;
    }
    int result = TINY_SUCCESS;
    tiny_mutex_lock(&handle->frames.mutex);
    tiny_frame_header_t frame = {
        .address = __peer_to_address_field( handle, peer ) | HDLC_CR_BIT,
        .control = HDLC_U_FRAME_TYPE_DISC | HDLC_U_FRAME_BITS,
    };
    if ( __put_u_s_frame_to_tx_queue(handle, TINY_FD_QUEUE_U_FRAME, &frame, 2) == NULL )
    {
        result = TINY_ERR_FAILED;
    }
    else
    {
        FILE_LOG((uintptr_t)handle, "OUT", 'U', "DISC", 0, 0);
        handle->peers[peer].state = TINY_FD_STATE_DISCONNECTING;
    }
    tiny_mutex_unlock(&handle->frames.mutex);
    return result;
}

///////////////////////////////////////////////////////////////////////////////

int tiny_fd_register_peer(tiny_fd_handle_t handle, uint8_t address)
{
    if ( address > 63 )
    {
        return TINY_ERR_FAILED;
    }
    address = (address << 2) | HDLC_E_BIT;
    if ( address == ( HDLC_PRIMARY_ADDR | HDLC_E_BIT ) )
    {
        return TINY_ERR_FAILED;
    }
    tiny_mutex_lock(&handle->frames.mutex);
    if ( __address_field_to_peer( handle, address ) != 0xFF )
    {
        tiny_mutex_unlock(&handle->frames.mutex);
        return TINY_ERR_FAILED;
    }
    // Attempt to register new secondary peer station
    for ( uint8_t peer = 0; peer < handle->peers_count; peer++ )
    {
        if ( handle->peers[peer].addr == 0xFF )
        {
            handle->peers[peer].addr = address;
            handle->peers[peer].last_ka_ts = (uint32_t)(tiny_millis() - handle->retry_timeout);
            tiny_mutex_unlock(&handle->frames.mutex);
            return TINY_SUCCESS;
        }
    }
    tiny_mutex_unlock(&handle->frames.mutex);
    return TINY_ERR_FAILED;
}

///////////////////////////////////////////////////////////////////////////////

