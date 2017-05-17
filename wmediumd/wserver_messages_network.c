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

#include <netinet/in.h>
#include <errno.h>
#include "wserver_messages_network.h"


int sendfull(int sock, const void *buf, size_t len, size_t shift, int flags) {
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t currsent = 0;
    while (total < len) {
        currsent = send(sock, buf + shift + total, bytesleft, flags);
        if (currsent == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return WACTION_DISCONNECTED;
            } else {
                return -errno;
            }
        }
        total += currsent;
        bytesleft -= currsent;
    }
    return WACTION_CONTINUE;
}

int recvfull(int sock, void *buf, size_t len, size_t shift, int flags) {
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t currrecv = 0;
    while (total < len) {
        currrecv = recv(sock, buf + shift + total, bytesleft, flags);
        if (currrecv == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return WACTION_DISCONNECTED;
            } else {
                return -errno;
            }
        } else if (currrecv == 0) {
            return WACTION_DISCONNECTED;
        }
        total += currrecv;
        bytesleft -= currrecv;
    }
    return WACTION_CONTINUE;
}

void htonu_wrapper(u32 *value) {
    *value = htonl(*value);
}

void ntohu_wrapper(u32 *value) {
    *value = ntohl(*value);
}

void htoni_wrapper(i32 *value) {
    *value = htonl(*value);
}

void ntohi_wrapper(i32 *value) {
    *value = ntohl(*value);
}

void hton_base(wserver_msg *elem) {
    UNUSED(elem);
}

void hton_snr_update_request(snr_update_request *elem) {
    hton_base(&elem->base);
    htoni_wrapper(&elem->snr);
}

void hton_snr_update_response(snr_update_response *elem) {
    hton_base(&elem->base);
    hton_snr_update_request(&elem->request);
}

void hton_errprob_update_request(errprob_update_request *elem) {
    hton_base(&elem->base);
    htonu_wrapper(&elem->errprob);
}

void hton_errprob_update_response(errprob_update_response *elem) {
    hton_base(&elem->base);
    hton_errprob_update_request(&elem->request);
}

void hton_station_del_by_mac_request(station_del_by_mac_request *elem) {
    hton_base(&elem->base);
}

void hton_station_del_by_mac_response(station_del_by_mac_response *elem) {
    hton_base(&elem->base);
    hton_station_del_by_mac_request(&elem->request);
}

void hton_station_del_by_id_request(station_del_by_id_request *elem) {
    hton_base(&elem->base);
    htoni_wrapper(&elem->id);
}

void hton_station_del_by_id_response(station_del_by_id_response *elem) {
    hton_base(&elem->base);
    hton_station_del_by_id_request(&elem->request);
}

void hton_station_add_request(station_add_request *elem) {
    hton_base(&elem->base);
}

void hton_station_add_response(station_add_response *elem) {
    hton_base(&elem->base);
    hton_station_add_request(&elem->request);
    htoni_wrapper(&elem->created_id);
}

void ntoh_base(wserver_msg *elem) {
    UNUSED(elem);
}

void ntoh_snr_update_request(snr_update_request *elem) {
    ntoh_base(&elem->base);
    ntohi_wrapper(&elem->snr);
}

void ntoh_snr_update_response(snr_update_response *elem) {
    ntoh_base(&elem->base);
    ntoh_snr_update_request(&elem->request);
}

void ntoh_errprob_update_request(errprob_update_request *elem) {
    ntoh_base(&elem->base);
    ntohu_wrapper(&elem->errprob);
}

void ntoh_errprob_update_response(errprob_update_response *elem) {
    ntoh_base(&elem->base);
    ntoh_errprob_update_request(&elem->request);
}

void ntoh_station_del_by_mac_request(station_del_by_mac_request *elem) {
    ntoh_base(&elem->base);
}

void ntoh_station_del_by_mac_response(station_del_by_mac_response *elem) {
    ntoh_base(&elem->base);
    ntoh_station_del_by_mac_request(&elem->request);
}

void ntoh_station_del_by_id_request(station_del_by_id_request *elem) {
    ntoh_base(&elem->base);
    ntohi_wrapper(&elem->id);
}

void ntoh_station_del_by_id_response(station_del_by_id_response *elem) {
    ntoh_base(&elem->base);
    ntoh_station_del_by_id_request(&elem->request);
}

void ntoh_station_add_request(station_add_request *elem) {
    ntoh_base(&elem->base);
}

void ntoh_station_add_response(station_add_response *elem) {
    ntoh_base(&elem->base);
    ntoh_station_add_request(&elem->request);
    ntohi_wrapper(&elem->created_id);
}