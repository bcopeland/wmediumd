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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "wmediumd_dynamic.h"

#define DEFAULT_DYNAMIC_SNR -10
#define DEFAULT_DYNAMIC_ERRPROB 1.0

pthread_rwlock_t snr_lock = PTHREAD_RWLOCK_INITIALIZER;

#define swap_matrix(matrix_ptr, oldsize, newsize, elem_type, backup_ptr) \
    backup_ptr = malloc(sizeof(elem_type) * oldsize * oldsize); \
    memcpy(backup_ptr, matrix_ptr, sizeof(elem_type) * oldsize * oldsize); \
    free(matrix_ptr); \
    matrix_ptr = malloc(sizeof(elem_type) * newsize * newsize);

int add_station(struct wmediumd *ctx, const u8 addr[]) {
    struct station *sta_loop;
    list_for_each_entry(sta_loop, &ctx->stations, list) {
        if (memcmp(sta_loop->addr, addr, ETH_ALEN) == 0)
            return -EEXIST;
    }

    pthread_rwlock_wrlock(&snr_lock);
    size_t oldnum = (size_t) ctx->num_stas;
    size_t newnum = oldnum + 1;

    // Save old matrix and init new matrix
    union {
        int *old_snr_matrix;
        double *old_errprob_matrix;
    } matrizes;
    int ret;
    if (ctx->error_prob_matrix == NULL) {
        swap_matrix(ctx->snr_matrix, oldnum, newnum, int, matrizes.old_snr_matrix);
    } else {
        swap_matrix(ctx->error_prob_matrix, oldnum, newnum, double, matrizes.old_errprob_matrix);
    }

    // Copy old matrix
    for (size_t x = 0; x < oldnum; x++) {
        for (size_t y = 0; y < oldnum; y++) {
            if (ctx->error_prob_matrix == NULL) {
                ctx->snr_matrix[x * newnum + y] = matrizes.old_snr_matrix[x * oldnum + y];
            } else {
                ctx->error_prob_matrix[x * newnum + y] = matrizes.old_errprob_matrix[x * oldnum + y];
            }
        }
    }

    // Fill last lines with default snr
    for (size_t x = 0; x < newnum; x++) {
        if (ctx->error_prob_matrix == NULL) {
            ctx->snr_matrix[x * newnum + oldnum] = DEFAULT_DYNAMIC_SNR;
        } else {
            ctx->error_prob_matrix[x * newnum + oldnum] = DEFAULT_DYNAMIC_ERRPROB;
        }
    }
    for (size_t y = 0; y < newnum; y++) {
        if (ctx->error_prob_matrix == NULL) {
            ctx->snr_matrix[oldnum * newnum + y] = DEFAULT_DYNAMIC_SNR;
        } else {
            ctx->error_prob_matrix[oldnum * newnum + y] = DEFAULT_DYNAMIC_ERRPROB;
        }
    }


    if (ctx->error_prob_matrix == NULL) {
        free(matrizes.old_snr_matrix);
    } else {
        free(matrizes.old_errprob_matrix);
    }

    // Init new station object
    struct station *station;
    station = malloc(sizeof(*station));
    if (!station) {
        ret = -ENOMEM;
        goto out;
    }
    station->index = (int) oldnum;
    memcpy(station->addr, addr, ETH_ALEN);
    memcpy(station->hwaddr, addr, ETH_ALEN);
    station_init_queues(station);
    list_add_tail(&station->list, &ctx->stations);
    ctx->num_stas = (int) newnum;
    ret = station->index;

    out:
    pthread_rwlock_unlock(&snr_lock);
    return ret;
}

int del_station(struct wmediumd *ctx, struct station *station) {
    if (ctx->num_stas == 0) {
        return -ENXIO;
    }
    size_t oldnum = (size_t) ctx->num_stas;
    size_t newnum = oldnum - 1;

    // Save old matrix and init new matrix
    union {
        int *old_snr_matrix;
        double *old_errprob_matrix;
    } matrizes;
    if (ctx->error_prob_matrix == NULL) {
        swap_matrix(ctx->snr_matrix, oldnum, newnum, int, matrizes.old_snr_matrix);
    } else {
        swap_matrix(ctx->error_prob_matrix, oldnum, newnum, double, matrizes.old_errprob_matrix);
    }

    size_t index = (size_t) station->index;

    // Decreasing index of stations following deleted station
    struct station *sta_loop = station;
    list_for_each_entry_from(sta_loop, &ctx->stations, list) {
        sta_loop->index = sta_loop->index - 1;
    }

    // Copy all values not related to deleted station
    int xnew = 0;
    for (size_t x = 0; x < oldnum; x++) {
        if (x == index) {
            continue;
        }
        int ynew = 0;
        for (size_t y = 0; y < oldnum; y++) {
            if (y == index) {
                continue;
            }
            if (ctx->error_prob_matrix == NULL) {
                ctx->snr_matrix[xnew * newnum + ynew] = matrizes.old_snr_matrix[x * oldnum + y];
            } else {
                ctx->error_prob_matrix[xnew * newnum + ynew] = matrizes.old_errprob_matrix[x * oldnum + y];
            }
            ynew++;
        }
        xnew++;
    }
    if (ctx->error_prob_matrix == NULL) {
        free(matrizes.old_snr_matrix);
    } else {
        free(matrizes.old_errprob_matrix);
    }

    list_del(&station->list);
    ctx->num_stas = (int) newnum;

    free(station);
    return 0;
}

int del_station_by_id(struct wmediumd *ctx, const i32 id) {
    pthread_rwlock_wrlock(&snr_lock);
    int ret;
    struct station *station;
    list_for_each_entry(station, &ctx->stations, list) {
        if (station->index == id) {
            ret = del_station(ctx, station);
            goto out;
        }
    }

    out:
    ret = -ENODEV;
    pthread_rwlock_unlock(&snr_lock);
    return ret;
}

int del_station_by_mac(struct wmediumd *ctx, const u8 *addr) {
    pthread_rwlock_wrlock(&snr_lock);
    int ret;
    struct station *station;
    list_for_each_entry(station, &ctx->stations, list) {
        if (memcmp(addr, station->addr, ETH_ALEN) == 0) {
            ret = del_station(ctx, station);
            goto out;
        }
    }
    ret = -ENODEV;

    out:
    pthread_rwlock_unlock(&snr_lock);
    return ret;
}
