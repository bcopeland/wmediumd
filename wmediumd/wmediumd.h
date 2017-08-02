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

/**
 * enum hwsim_attrs - hwsim netlink attributes
 *
 * @HWSIM_ATTR_UNSPEC: unspecified attribute to catch errors
 *
 * @HWSIM_ATTR_ADDR_RECEIVER: MAC address of the radio device that
 *	the frame is broadcasted to
 * @HWSIM_ATTR_ADDR_TRANSMITTER: MAC address of the radio device that
 *	the frame was broadcasted from
 * @HWSIM_ATTR_FRAME: Data array
 * @HWSIM_ATTR_FLAGS: mac80211 transmission flags, used to process
	properly the frame at user space
 * @HWSIM_ATTR_RX_RATE: estimated rx rate index for this frame at user
	space
 * @HWSIM_ATTR_SIGNAL: estimated RX signal for this frame at user
	space
 * @HWSIM_ATTR_TX_INFO: ieee80211_tx_rate array
 * @HWSIM_ATTR_COOKIE: sk_buff cookie to identify the frame
 * @HWSIM_ATTR_CHANNELS: u32 attribute used with the %HWSIM_CMD_CREATE_RADIO
 *	command giving the number of channels supported by the new radio
 * @HWSIM_ATTR_RADIO_ID: u32 attribute used with %HWSIM_CMD_DESTROY_RADIO
 *	only to destroy a radio
 * @HWSIM_ATTR_REG_HINT_ALPHA2: alpha2 for regulatoro driver hint
 *	(nla string, length 2)
 * @HWSIM_ATTR_REG_CUSTOM_REG: custom regulatory domain index (u32 attribute)
 * @HWSIM_ATTR_REG_STRICT_REG: request REGULATORY_STRICT_REG (flag attribute)
 * @HWSIM_ATTR_SUPPORT_P2P_DEVICE: support P2P Device virtual interface (flag)
 * @HWSIM_ATTR_USE_CHANCTX: used with the %HWSIM_CMD_CREATE_RADIO
 *	command to force use of channel contexts even when only a
 *	single channel is supported
 * @HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE: used with the %HWSIM_CMD_CREATE_RADIO
 *	command to force radio removal when process that created the radio dies
 * @HWSIM_ATTR_RADIO_NAME: Name of radio, e.g. phy666
 * @HWSIM_ATTR_NO_VIF:  Do not create vif (wlanX) when creating radio.
 * @HWSIM_ATTR_FREQ: Frequency at which packet is transmitted or received.
 * @__HWSIM_ATTR_MAX: enum limit
 */


enum {
	HWSIM_ATTR_UNSPEC,
	HWSIM_ATTR_ADDR_RECEIVER,
	HWSIM_ATTR_ADDR_TRANSMITTER,
	HWSIM_ATTR_FRAME,
	HWSIM_ATTR_FLAGS,
	HWSIM_ATTR_RX_RATE,
	HWSIM_ATTR_SIGNAL,
	HWSIM_ATTR_TX_INFO,
	HWSIM_ATTR_COOKIE,
	HWSIM_ATTR_CHANNELS,
	HWSIM_ATTR_RADIO_ID,
	HWSIM_ATTR_REG_HINT_ALPHA2,
	HWSIM_ATTR_REG_CUSTOM_REG,
	HWSIM_ATTR_REG_STRICT_REG,
	HWSIM_ATTR_SUPPORT_P2P_DEVICE,
	HWSIM_ATTR_USE_CHANCTX,
	HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE,
	HWSIM_ATTR_RADIO_NAME,
	HWSIM_ATTR_NO_VIF,
	HWSIM_ATTR_FREQ,
	HWSIM_ATTR_PAD,
	__HWSIM_ATTR_MAX,
};
#define HWSIM_ATTR_MAX (__HWSIM_ATTR_MAX - 1)

#define VERSION_NR 1

#define SNR_DEFAULT 30

#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>

#include "list.h"
#include "ieee80211.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

#define TIME_FMT "%lld.%06lld"
#define TIME_ARGS(a) ((unsigned long long)(a)->tv_sec), ((unsigned long long)(a)->tv_nsec/1000)

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARGS(a) a[0],a[1],a[2],a[3],a[4],a[5]

#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif

#define NOISE_LEVEL	(-91)
#define CCA_THRESHOLD	(-90)

struct wqueue {
	struct list_head frames;
	int cw_min;
	int cw_max;
};

struct station {
	int index;
	u8 addr[ETH_ALEN];		/* virtual interface mac address */
	u8 hwaddr[ETH_ALEN];		/* hardware address of hwsim radio */
	double x, y;			/* position of the station [m] */
	double dir_x, dir_y;		/* direction of the station [meter per MOVE_INTERVAL] */
	int tx_power;			/* transmission power [dBm] */
	struct wqueue queues[IEEE80211_NUM_ACS];
	struct list_head list;
};

struct wmediumd {
	int timerfd;

	struct nl_sock *sock;

	int num_stas;
	struct list_head stations;
	struct station **sta_array;
	int *snr_matrix;
	double *error_prob_matrix;
	struct intf_info *intf;
	struct timespec intf_updated;
#define MOVE_INTERVAL	(3) /* station movement interval [sec] */
	struct timespec next_move;
	void *path_loss_param;
	float *per_matrix;
	int per_matrix_row_num;
	int per_matrix_signal_min;
	int fading_coefficient;

	struct nl_cb *cb;
	int family_id;

	int (*get_link_snr)(struct wmediumd *, struct station *,
			    struct station *);
	double (*get_error_prob)(struct wmediumd *, double, unsigned int, u32,
				 int, struct station *, struct station *);
	int (*calc_path_loss)(void *, struct station *,
			      struct station *);
	void (*move_stations)(struct wmediumd *);
	int (*get_fading_signal)(struct wmediumd *);

	u8 log_lvl;
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
	u32 freq;
	int flags;
	int signal;
	int duration;
	int tx_rates_count;
	struct station *sender;
	struct hwsim_tx_rate tx_rates[IEEE80211_TX_MAX_RATES];
	size_t data_len;
	u8 data[0];			/* frame contents */
};

struct log_distance_model_param {
	double path_loss_exponent;
	double Xg;
};

struct itu_model_param {
	int nFLOORS;
	int LF;
};

struct intf_info {
	int signal;
	int duration;
	double prob_col;
};

void station_init_queues(struct station *station);
double get_error_prob_from_snr(double snr, unsigned int rate_idx, u32 freq,
			       int frame_len);
bool timespec_before(struct timespec *t1, struct timespec *t2);
int set_default_per(struct wmediumd *ctx);
int read_per_file(struct wmediumd *ctx, const char *file_name);
int w_logf(struct wmediumd *ctx, u8 level, const char *format, ...);
int w_flogf(struct wmediumd *ctx, u8 level, FILE *stream, const char *format, ...);
int index_to_rate(size_t index, u32 freq);

#endif /* WMEDIUMD_H_ */
