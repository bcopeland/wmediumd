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

#ifndef WMEDIUMD_WMEDIUMD_DYNAMIC_H
#define WMEDIUMD_WMEDIUMD_DYNAMIC_H

#include <stdint.h>
#include <pthread.h>
#include "wmediumd.h"

typedef uint8_t u8;
typedef int32_t i32;

/**
 * Add a station
 * @param ctx The wmediumd context
 * @param addr The MAC address of the station
 * @return The station ID or a negative errno value
 */
int add_station(struct wmediumd *ctx, const u8 addr[]);

/**
 * Delete a station
 * @param ctx The wmediumd context
 * @param station The station to delete
 * @return 0 on success otherwise a negative errno value
 */
int del_station(struct wmediumd *ctx, struct station *station);

/**
 * Delete a station by its id
 * @param ctx The wmediumd context
 * @param id The ID of the station
 * @return 0 on success otherwise a negative errno value
 */
int del_station_by_id(struct wmediumd *ctx, const i32 id);

/**
 * Delete a station by its address
 * @param ctx The wmediumd context
 * @param addr The MAC address of the station
 * @return 0 on success otherwise a negative errno value
 */
int del_station_by_mac(struct wmediumd *ctx, const u8 *addr);

/**
 * Lock for the snr matrix/station list
 */
extern pthread_rwlock_t snr_lock;

#endif //WMEDIUMD_WMEDIUMD_DYNAMIC_H
