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

#include "../wmediumd/wserver_messages.h"
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/socket.h>


#define send_request(connection_soc, request, type) \
    { \
        int ret = wserver_send_msg(connection_soc, request, type); \
        if (ret < 0) { \
            perror("error while sending"); \
            close(connection_soc); \
            exit(EXIT_FAILURE); \
        } \
        printf("sent request\n"); \
    }


#define receive_response(connection_soc, response, elemtype, typeint) \
    { \
    wserver_msg base; \
    int recv_type; \
    int ret = wserver_recv_msg_base(connection_soc, &base, &recv_type); \
    if (ret < 0) { \
        perror("error while receiving"); \
        close(connection_soc); \
        exit(EXIT_FAILURE); \
    } \
    if (recv_type != typeint) { \
        fprintf(stderr, "Received invalid request of type %d", recv_type); \
        close(connection_soc); \
        exit(EXIT_FAILURE); \
    } \
    ret = wserver_recv_msg(connection_soc, response, elemtype); \
    if (ret < 0) { \
        perror("error while receiving"); \
        close(connection_soc); \
        exit(EXIT_FAILURE); \
    } \
    printf("received response of type %d\n", typeint); \
    }

void string_to_mac_address(const char *str, u8 *addr) {
    int a[ETH_ALEN];

    sscanf(str, "%x:%x:%x:%x:%x:%x",
           &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);

    addr[0] = (u8) a[0];
    addr[1] = (u8) a[1];
    addr[2] = (u8) a[2];
    addr[3] = (u8) a[3];
    addr[4] = (u8) a[4];
    addr[5] = (u8) a[5];
}

u32 double_errprob_to_fixed_point(double errprob) {
    unsigned int SHIFT_AMOUNT = 31;
    u32 beforecomma = (unsigned int) errprob;
    u32 aftercomma = (unsigned int) ((errprob - beforecomma) * (1 << SHIFT_AMOUNT));
    return (beforecomma << SHIFT_AMOUNT) + aftercomma;
}

int main() {
    int create_socket;
    struct sockaddr_un address;
    if ((create_socket = socket(AF_UNIX, SOCK_STREAM, 0)) > 0) {
        printf("Socket has been created\n");
    } else {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, WSERVER_SOCKET_PATH);
    if (connect(create_socket,
                (struct sockaddr *) &address,
                sizeof(address)) == 0) {
        printf("Connected to server\n");

        printf("==== station add\n");
        station_add_request request;
        string_to_mac_address("02:00:00:00:02:00", request.addr);
        send_request(create_socket, &request, station_add_request);
        station_add_response response;
        receive_response(create_socket, &response, station_add_response, WSERVER_ADD_RESPONSE_TYPE);
        printf("answer was: %d\n", response.update_result);

        printf("==== errprob update 1\n");
        errprob_update_request request2;
        string_to_mac_address("02:00:00:00:01:00", request2.from_addr);
        string_to_mac_address("02:00:00:00:02:00", request2.to_addr);
        request2.errprob = double_errprob_to_fixed_point(0.7);
        send_request(create_socket, &request2, errprob_update_request);
        errprob_update_response response2;
        receive_response(create_socket, &response2, errprob_update_response, WSERVER_ERRPROB_UPDATE_RESPONSE_TYPE);
        printf("answer was: %d\n", response2.update_result);

        printf("==== errprob update 2\n");
        errprob_update_request request3;
        string_to_mac_address("02:00:00:00:02:00", request3.from_addr);
        string_to_mac_address("02:00:00:00:01:00", request3.to_addr);
        request3.errprob = double_errprob_to_fixed_point(0.5);
        send_request(create_socket, &request3, errprob_update_request);
        errprob_update_response response3;
        receive_response(create_socket, &response3, errprob_update_response, WSERVER_ERRPROB_UPDATE_RESPONSE_TYPE);
        printf("answer was: %d\n", response3.update_result);


        printf("==== station del\n");
        station_del_by_mac_request request4;
        string_to_mac_address("02:00:00:00:02:00", request4.addr);
        send_request(create_socket, &request4, station_del_by_mac_request);
        station_del_by_mac_response response4;
        receive_response(create_socket, &response4, station_del_by_mac_response, WSERVER_DEL_BY_MAC_RESPONSE_TYPE);
        printf("answer was: %d\n", response4.update_result);

        close(create_socket);
        printf("socket closed\n");
        return EXIT_SUCCESS;
    } else {
        perror("Server connection failed");
        return EXIT_FAILURE;
    }
}