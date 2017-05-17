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

#ifndef WMEDIUMD_WSERVER_MESSAGES_H
#define WMEDIUMD_WSERVER_MESSAGES_H

#include <stdint.h>
#include <unistd.h>
#include "ieee80211.h"

#define WACTION_CONTINUE 0 /* Operation successful, continue */
#define WACTION_ERROR 1 /* Error occured, disconnect client */
#define WACTION_DISCONNECTED 2 /* Client has disconnected */
#define WACTION_CLOSE 3 /* Close the server */

#define WUPDATE_SUCCESS 0 /* update of SNR successful */
#define WUPDATE_INTF_NOTFOUND 1 /* unknown interface */
#define WUPDATE_INTF_DUPLICATE 2 /* interface already exists */
#define WUPDATE_WRONG_MODE 3 /* tried to update snr in errprob mode or vice versa */

/* Socket location following FHS guidelines:
 * http://www.pathname.com/fhs/pub/fhs-2.3.html#PURPOSE46 */
#define WSERVER_SOCKET_PATH "/var/run/wmediumd.sock"

#define WSERVER_SHUTDOWN_REQUEST_TYPE 0
#define WSERVER_SNR_UPDATE_REQUEST_TYPE 1
#define WSERVER_SNR_UPDATE_RESPONSE_TYPE 2
#define WSERVER_DEL_BY_MAC_REQUEST_TYPE 3
#define WSERVER_DEL_BY_MAC_RESPONSE_TYPE 4
#define WSERVER_DEL_BY_ID_REQUEST_TYPE 5
#define WSERVER_DEL_BY_ID_RESPONSE_TYPE 6
#define WSERVER_ADD_REQUEST_TYPE 7
#define WSERVER_ADD_RESPONSE_TYPE 8
#define WSERVER_ERRPROB_UPDATE_REQUEST_TYPE 9
#define WSERVER_ERRPROB_UPDATE_RESPONSE_TYPE 10

#ifndef __packed
#define __packed __attribute__((packed))
#endif

typedef uint8_t u8;
typedef int32_t i32;
typedef uint32_t u32;

/*
 * Macro for unused parameters
 */
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

typedef struct __packed {
    u8 type;
} wserver_msg;

typedef struct __packed {
    wserver_msg base;
    u8 from_addr[ETH_ALEN];
    u8 to_addr[ETH_ALEN];
    i32 snr;
} snr_update_request;

typedef struct __packed {
    wserver_msg base;
    snr_update_request request;
    u8 update_result;
} snr_update_response;

typedef struct __packed {
    wserver_msg base;
    u8 from_addr[ETH_ALEN];
    u8 to_addr[ETH_ALEN];
    u32 errprob;
} errprob_update_request;

typedef struct __packed {
    wserver_msg base;
    errprob_update_request request;
    u8 update_result;
} errprob_update_response;

typedef struct __packed {
    wserver_msg base;
    u8 addr[ETH_ALEN];
} station_del_by_mac_request;

typedef struct __packed {
    wserver_msg base;
    station_del_by_mac_request request;
    u8 update_result;
} station_del_by_mac_response;

typedef struct __packed {
    wserver_msg base;
    i32 id;
} station_del_by_id_request;

typedef struct __packed {
    wserver_msg base;
    station_del_by_id_request request;
    u8 update_result;
} station_del_by_id_response;

typedef struct __packed {
    wserver_msg base;
    u8 addr[ETH_ALEN];
} station_add_request;

typedef struct __packed {
    wserver_msg base;
    station_add_request request;
    i32 created_id;
    u8 update_result;
} station_add_response;

/**
 * Receive the wserver_msg from a socket
 * @param sock_fd The socket file descriptor
 * @param base Where to store the wserver_msg
 * @param recv_type The received WSERVER_*_TYPE
 * @return A positive WACTION_* constant, or a negative errno value
 */
int wserver_recv_msg_base(int sock_fd, wserver_msg *base, int *recv_type);

/**
 * Send a wserver message to a socket
 * @param sock_fd The socket file descriptor
 * @param elem The message to send
 * @param type The response type struct
 * @return 0 on success
 */
#define wserver_send_msg(sock_fd, elem, type) \
    send_##type(sock_fd, elem)

/**
 * Receive a wserver msg from a socket
 * @param sock_fd The socket file descriptor
 * @param elem Where to store the msg
 * @param type The response type struct
 * @return A positive WACTION_* constant, or a negative errno value
 */
#define wserver_recv_msg(sock_fd, elem, type) \
    recv_##type(sock_fd, elem)

/**
 * Get the size of a request/response based on its type
 * @param type The WSERVER_*_TYPE
 * @return The size or -1 if not found
 */
ssize_t get_msg_size_by_type(int type);

int send_snr_update_request(int sock, const snr_update_request *elem);

int send_snr_update_response(int sock, const snr_update_response *elem);

int send_errprob_update_request(int sock, const errprob_update_request *elem);

int send_errprob_update_response(int sock, const errprob_update_response *elem);

int send_station_del_by_mac_request(int sock, const station_del_by_mac_request *elem);

int send_station_del_by_mac_response(int sock, const station_del_by_mac_response *elem);

int send_station_del_by_id_request(int sock, const station_del_by_id_request *elem);

int send_station_del_by_id_response(int sock, const station_del_by_id_response *elem);

int send_station_add_request(int sock, const station_add_request *elem);

int send_station_add_response(int sock, const station_add_response *elem);

int recv_snr_update_request(int sock, snr_update_request *elem);

int recv_snr_update_response(int sock, snr_update_response *elem);

int recv_errprob_update_request(int sock, errprob_update_request *elem);

int recv_errprob_update_response(int sock, errprob_update_response *elem);

int recv_station_del_by_mac_request(int sock, station_del_by_mac_request *elem);

int recv_station_del_by_mac_response(int sock, station_del_by_mac_response *elem);

int recv_station_del_by_id_request(int sock, station_del_by_id_request *elem);

int recv_station_del_by_id_response(int sock, station_del_by_id_response *elem);

int recv_station_add_request(int sock, station_add_request *elem);

int recv_station_add_response(int sock, station_add_response *elem);

#endif //WMEDIUMD_WSERVER_MESSAGES_H
