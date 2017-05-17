/*
 *	wmediumd_server - server for on-the-fly modifications for wmediumd
 *	Copyright (c) 2016, Patrick Grosse <patrick.grosse@uni-muenster.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version 2
 *	of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *	02110-1301, USA.
 */

#ifndef WMEDIUMD_WSERVER_MESSAGES_NETWORK_H
#define WMEDIUMD_WSERVER_MESSAGES_NETWORK_H

#include "wserver_messages.h"

/**
 * Send bytes over a socket, repeat until all bytes are sent
 * @param sock The socket file descriptor
 * @param buf The pointer to the bytes
 * @param len The amount of bytes to send
 * @param shift The amount of bytes that should be skipped in the buffer
 * @param flags Flags for the send method
 * @return 0 on success, -1 on error, -2 on client disconnect
 */
int sendfull(int sock, const void *buf, size_t len, size_t shift, int flags);

/**
 * Receive bytes from a socket, repeat until all bytes are read
 * @param sock The socket file descriptor
 * @param buf A pointer where to store the received bytes
 * @param len The amount of bytes to receive
 * @param shift The amount of bytes that should be skipped in the buffer
 * @param flags Flags for the recv method
 * @return 0 on success, -1 on error, -2 on client disconnect
 */
int recvfull(int sock, void *buf, size_t len, size_t shift, int flags);

/**
 * Convert a wserver message from network to host byte order
 * @param elem The element to convert
 * @param type The struct type of the element
 */
#define hton_type(elem, type) \
    hton_##type(elem);

/**
 * Convert a wserver message from host to network byte order
 * @param elem The element to convert
 * @param type The struct type of the element
 */
#define ntoh_type(elem, type) \
    ntoh_##type(elem);

void hton_base(wserver_msg *elem);

void hton_snr_update_request(snr_update_request *elem);

void hton_snr_update_response(snr_update_response *elem);

void hton_errprob_update_request(errprob_update_request *elem);

void hton_errprob_update_response(errprob_update_response *elem);

void hton_station_del_by_mac_request(station_del_by_mac_request *elem);

void hton_station_del_by_mac_response(station_del_by_mac_response *elem);

void hton_station_del_by_id_request(station_del_by_id_request *elem);

void hton_station_del_by_id_response(station_del_by_id_response *elem);

void hton_station_add_request(station_add_request *elem);

void hton_station_add_response(station_add_response *elem);

void ntoh_base(wserver_msg *elem);

void ntoh_snr_update_request(snr_update_request *elem);

void ntoh_snr_update_response(snr_update_response *elem);

void ntoh_errprob_update_request(errprob_update_request *elem);

void ntoh_errprob_update_response(errprob_update_response *elem);

void ntoh_station_del_by_mac_request(station_del_by_mac_request *elem);

void ntoh_station_del_by_mac_response(station_del_by_mac_response *elem);

void ntoh_station_del_by_id_request(station_del_by_id_request *elem);

void ntoh_station_del_by_id_response(station_del_by_id_response *elem);

void ntoh_station_add_request(station_add_request *elem);

void ntoh_station_add_response(station_add_response *elem);

#endif //WMEDIUMD_WSERVER_MESSAGES_NETWORK_H
