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

#ifndef WMEDIUMD_SERVER_H
#define WMEDIUMD_SERVER_H

#include "wmediumd.h"
#include "wserver_messages.h"

struct request_ctx {
    struct wmediumd *ctx;
    int sock_fd;
};

/**
 * Start the server using the given wmediumd context in a background task
 * @param ctx The wmediumd context
 * @return 0 on success
 */
int start_wserver(struct wmediumd *ctx);

/**
 * Stop the server if active
 */
void stop_wserver();

/**
 * Handle a snr_update_request and pass it to wmediumd
 * @param ctx The request_ctx context
 * @param request The received request
 */
int handle_snr_update_request(struct request_ctx *ctx, const snr_update_request *request);

/**
 * Handle a errprob_update_request and pass it to wmediumd
 * @param ctx The request_ctx context
 * @param request The received request
 */
int handle_errprob_update_request(struct request_ctx *ctx, const errprob_update_request *request);

/**
 * Handle a station_del_by_id_request and pass it to wmediumd
 * @param ctx The request_ctx context
 * @param request The received request
 */
int handle_delete_by_id_request(struct request_ctx *ctx, station_del_by_id_request *request);

/**
 * Handle a station_del_by_mac_request and pass it to wmediumd
 * @param ctx The request_ctx context
 * @param request The received request
 */
int handle_delete_by_mac_request(struct request_ctx *ctx, station_del_by_mac_request *request);

/**
 * Handle a station_add_request and pass it to wmediumd
 * @param ctx The request_ctx context
 * @param request The received request
 */
int handle_add_request(struct request_ctx *ctx, station_add_request *request);

#endif //WMEDIUMD_SERVER_H
