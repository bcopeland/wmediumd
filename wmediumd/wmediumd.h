/*
 *	wmediumd, wireless medium simulator for mac80211_hwsim kernel module
 *	Copyright (c) 2011 cozybit Inc.
 *
 *	Author:	Javier Lopez	<jlopex@cozybit.com>
 *		Javier Cardona	<javier@cozybit.com>
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

#ifndef WMEDIUMD_H_
#define WMEDIUMD_H_

#define HWSIM_TX_CTL_REQ_TX_STATUS	1
#define HWSIM_TX_CTL_NO_ACK		(1 << 1)
#define HWSIM_TX_STAT_ACK		(1 << 2)

#define HWSIM_CMD_REGISTER 1
#define HWSIM_CMD_FRAME 2
#define HWSIM_CMD_TX_INFO_FRAME 3

#define HWSIM_ATTR_ADDR_RECEIVER 1
#define HWSIM_ATTR_ADDR_TRANSMITTER 2
#define HWSIM_ATTR_FRAME 3
#define HWSIM_ATTR_FLAGS 4
#define HWSIM_ATTR_RX_RATE 5
#define HWSIM_ATTR_SIGNAL 6
#define HWSIM_ATTR_TX_INFO 7
#define HWSIM_ATTR_COOKIE 8
#define HWSIM_ATTR_MAX 8
#define VERSION_NR 1

#define SNR_DEFAULT 30

#include <stdint.h>
#include <stdbool.h>

#include "list.h"
#include "ieee80211.h"

typedef uint8_t u8;
typedef uint64_t u64;

#define TIME_FMT "%lld.%06lld"
#define TIME_ARGS(a) ((unsigned long long)(a)->tv_sec), ((unsigned long long)(a)->tv_nsec/1000)

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARGS(a) a[0],a[1],a[2],a[3],a[4],a[5]

#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif

struct wqueue {
	struct list_head frames;
	int cw_min;
	int cw_max;
};

struct station {
	int index;
	u8 addr[ETH_ALEN];		/* virtual interface mac address */
	u8 hwaddr[ETH_ALEN];		/* hardware address of hwsim radio */
	struct wqueue queues[IEEE80211_NUM_ACS];
	struct list_head list;
};

struct wmediumd {
	int timerfd;

	struct nl_sock *sock;

	int num_stas;
	struct list_head stations;
	int *snr_matrix;
	double *error_prob_matrix;

	struct nl_cb *cb;
	struct nl_cache *cache;
	struct genl_family *family;

	int (*get_link_snr)(struct wmediumd *, struct station *,
			    struct station *);
	double (*get_error_prob)(struct wmediumd *, double, unsigned int, int,
				 struct station *, struct station *);
};

struct hwsim_tx_rate {
	signed char idx;
	unsigned char count;
};

struct frame {
	struct list_head list;		/* frame queue list */
	struct timespec expires;	/* frame delivery (absolute) */
	bool acked;
	u64 cookie;
	int flags;
	int signal;
	int tx_rates_count;
	struct station *sender;
	struct hwsim_tx_rate tx_rates[IEEE80211_TX_MAX_RATES];
	size_t data_len;
	u8 data[0];			/* frame contents */
};

void station_init_queues(struct station *station);
double get_error_prob_from_snr(double snr, unsigned int rate_idx,
			       int frame_len);

#endif /* WMEDIUMD_H_ */
