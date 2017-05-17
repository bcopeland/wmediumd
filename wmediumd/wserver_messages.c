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

#include <sys/socket.h>
#include <memory.h>
#include "wserver_messages.h"
#include "wserver_messages_network.h"


#define align_send_msg(sock_fd, elem, type, typeint) \
    type tosend; \
    *((int *) elem) = typeint; \
    memcpy(&tosend, elem, sizeof(type)); \
    hton_type(&tosend, type);\
    return sendfull(sock_fd, &tosend, sizeof(type), 0, MSG_NOSIGNAL);

#define align_recv_msg(sock_fd, elem, elemtype, typeint) \
    int ret; \
    ret = recvfull(sock_fd, elem, sizeof(elemtype) - sizeof(wserver_msg), sizeof(wserver_msg), 0); \
    ntoh_type(elem, elemtype); \
    elem->base.type = typeint; \
    return ret;


int send_snr_update_request(int sock, const snr_update_request *elem) {
    align_send_msg(sock, elem, snr_update_request, WSERVER_SNR_UPDATE_REQUEST_TYPE)
}

int send_snr_update_response(int sock, const snr_update_response *elem) {
    align_send_msg(sock, elem, snr_update_response, WSERVER_SNR_UPDATE_RESPONSE_TYPE)
}

int send_errprob_update_request(int sock, const errprob_update_request *elem) {
    align_send_msg(sock, elem, errprob_update_request, WSERVER_ERRPROB_UPDATE_REQUEST_TYPE)
}

int send_errprob_update_response(int sock, const errprob_update_response *elem) {
    align_send_msg(sock, elem, errprob_update_response, WSERVER_ERRPROB_UPDATE_RESPONSE_TYPE)
}

int send_station_del_by_mac_request(int sock, const station_del_by_mac_request *elem) {
    align_send_msg(sock, elem, station_del_by_mac_request, WSERVER_DEL_BY_MAC_REQUEST_TYPE)
}

int send_station_del_by_mac_response(int sock, const station_del_by_mac_response *elem) {
    align_send_msg(sock, elem, station_del_by_mac_response, WSERVER_DEL_BY_MAC_RESPONSE_TYPE)
}

int send_station_del_by_id_request(int sock, const station_del_by_id_request *elem) {
    align_send_msg(sock, elem, station_del_by_id_request, WSERVER_DEL_BY_ID_REQUEST_TYPE)
}

int send_station_del_by_id_response(int sock, const station_del_by_id_response *elem) {
    align_send_msg(sock, elem, station_del_by_id_response, WSERVER_DEL_BY_ID_RESPONSE_TYPE)
}

int send_station_add_request(int sock, const station_add_request *elem) {
    align_send_msg(sock, elem, station_add_request, WSERVER_ADD_REQUEST_TYPE)
}

int send_station_add_response(int sock, const station_add_response *elem) {
    align_send_msg(sock, elem, station_add_response, WSERVER_ADD_RESPONSE_TYPE)
}

int recv_snr_update_request(int sock, snr_update_request *elem) {
    align_recv_msg(sock, elem, snr_update_request, WSERVER_SNR_UPDATE_REQUEST_TYPE)
}

int recv_snr_update_response(int sock, snr_update_response *elem) {
    align_recv_msg(sock, elem, snr_update_response, WSERVER_SNR_UPDATE_RESPONSE_TYPE)
}

int recv_errprob_update_request(int sock, errprob_update_request *elem) {
    align_recv_msg(sock, elem, errprob_update_request, WSERVER_ERRPROB_UPDATE_REQUEST_TYPE)
}

int recv_errprob_update_response(int sock, errprob_update_response *elem) {
    align_recv_msg(sock, elem, errprob_update_response, WSERVER_ERRPROB_UPDATE_RESPONSE_TYPE)
}

int recv_station_del_by_mac_request(int sock, station_del_by_mac_request *elem) {
    align_recv_msg(sock, elem, station_del_by_mac_request, WSERVER_DEL_BY_MAC_REQUEST_TYPE)
}

int recv_station_del_by_mac_response(int sock, station_del_by_mac_response *elem) {
    align_recv_msg(sock, elem, station_del_by_mac_response, WSERVER_DEL_BY_MAC_RESPONSE_TYPE)
}

int recv_station_del_by_id_request(int sock, station_del_by_id_request *elem) {
    align_recv_msg(sock, elem, station_del_by_id_request, WSERVER_DEL_BY_ID_REQUEST_TYPE)
}

int recv_station_del_by_id_response(int sock, station_del_by_id_response *elem) {
    align_recv_msg(sock, elem, station_del_by_id_response, WSERVER_DEL_BY_ID_RESPONSE_TYPE)
}

int recv_station_add_request(int sock, station_add_request *elem) {
    align_recv_msg(sock, elem, station_add_request, WSERVER_ADD_REQUEST_TYPE)
}

int recv_station_add_response(int sock, station_add_response *elem) {
    align_recv_msg(sock, elem, station_add_response, WSERVER_ADD_RESPONSE_TYPE)
}

int wserver_recv_msg_base(int sock_fd, wserver_msg *base, int *recv_type) {
    int ret = recvfull(sock_fd, base, sizeof(wserver_msg), 0, 0);
    if (ret) {
        return ret;
    }
    ntoh_base(base);
    *recv_type = base->type;
    hton_base(base);
    return 0;
}

ssize_t get_msg_size_by_type(int type) {
    switch (type) {
        case WSERVER_SHUTDOWN_REQUEST_TYPE:
            return sizeof(wserver_msg);
        case WSERVER_SNR_UPDATE_REQUEST_TYPE:
            return sizeof(snr_update_request);
        case WSERVER_SNR_UPDATE_RESPONSE_TYPE:
            return sizeof(snr_update_response);
        case WSERVER_DEL_BY_MAC_REQUEST_TYPE:
            return sizeof(station_del_by_mac_request);
        case WSERVER_DEL_BY_MAC_RESPONSE_TYPE:
            return sizeof(station_del_by_mac_response);
        case WSERVER_DEL_BY_ID_REQUEST_TYPE:
            return sizeof(station_del_by_id_request);
        case WSERVER_DEL_BY_ID_RESPONSE_TYPE:
            return sizeof(station_del_by_id_response);
        case WSERVER_ADD_REQUEST_TYPE:
            return sizeof(station_add_request);
        case WSERVER_ADD_RESPONSE_TYPE:
            return sizeof(station_add_response);
        case WSERVER_ERRPROB_UPDATE_REQUEST_TYPE:
            return sizeof(errprob_update_request);
        case WSERVER_ERRPROB_UPDATE_RESPONSE_TYPE:
            return sizeof(errprob_update_response);
        default:
            return -1;
    }
}
