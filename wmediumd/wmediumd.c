/*
 *	wmediumd, wireless medium simulator for mac80211_hwsim kernel module
 *	Copyright (c) 2011 cozybit Inc.
 *	Copyright (C) 2020 Intel Corporation
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

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <math.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <usfstl/loop.h>
#include <usfstl/sched.h>
#include <usfstl/schedctrl.h>
#include <usfstl/vhost.h>

#include "wmediumd.h"
#include "ieee80211.h"
#include "config.h"

USFSTL_SCHEDULER(scheduler);

static void wmediumd_deliver_frame(struct usfstl_job *job);

enum {
	HWSIM_VQ_TX,
	HWSIM_VQ_RX,
	HWSIM_NUM_VQS,
};

static inline int div_round(int a, int b)
{
	return (a + b - 1) / b;
}

static inline int pkt_duration(int len, int rate)
{
	/* preamble + signal + t_sym * n_sym, rate in 100 kbps */
	return 16 + 4 + 4 * div_round((16 + 8 * len + 6) * 10, 4 * rate);
}

int w_logf(struct wmediumd *ctx, u8 level, const char *format, ...)
{
	va_list(args);
	va_start(args, format);
	if (ctx->log_lvl >= level) {
		return vprintf(format, args);
	}
	return -1;
}

int w_flogf(struct wmediumd *ctx, u8 level, FILE *stream, const char *format, ...)
{
	va_list(args);
	va_start(args, format);
	if (ctx->log_lvl >= level) {
		return vfprintf(stream, format, args);
	}
	return -1;
}

static void wqueue_init(struct wqueue *wqueue, int cw_min, int cw_max)
{
	INIT_LIST_HEAD(&wqueue->frames);
	wqueue->cw_min = cw_min;
	wqueue->cw_max = cw_max;
}

void station_init_queues(struct station *station)
{
	wqueue_init(&station->queues[IEEE80211_AC_BK], 15, 1023);
	wqueue_init(&station->queues[IEEE80211_AC_BE], 15, 1023);
	wqueue_init(&station->queues[IEEE80211_AC_VI], 7, 15);
	wqueue_init(&station->queues[IEEE80211_AC_VO], 3, 7);
}

static inline bool frame_has_a4(struct frame *frame)
{
	struct ieee80211_hdr *hdr = (void *)frame->data;

	return (hdr->frame_control[1] & (FCTL_TODS | FCTL_FROMDS)) ==
		(FCTL_TODS | FCTL_FROMDS);
}

static inline bool frame_is_mgmt(struct frame *frame)
{
	struct ieee80211_hdr *hdr = (void *)frame->data;

	return (hdr->frame_control[0] & FCTL_FTYPE) == FTYPE_MGMT;
}

static inline bool frame_is_data(struct frame *frame)
{
	struct ieee80211_hdr *hdr = (void *)frame->data;

	return (hdr->frame_control[0] & FCTL_FTYPE) == FTYPE_DATA;
}

static inline bool frame_is_data_qos(struct frame *frame)
{
	struct ieee80211_hdr *hdr = (void *)frame->data;

	return (hdr->frame_control[0] & (FCTL_FTYPE | STYPE_QOS_DATA)) ==
		(FTYPE_DATA | STYPE_QOS_DATA);
}

static inline u8 *frame_get_qos_ctl(struct frame *frame)
{
	struct ieee80211_hdr *hdr = (void *)frame->data;

	if (frame_has_a4(frame))
		return (u8 *)hdr + 30;
	else
		return (u8 *)hdr + 24;
}

static enum ieee80211_ac_number frame_select_queue_80211(struct frame *frame)
{
	u8 *p;
	int priority;

	if (!frame_is_data(frame))
		return IEEE80211_AC_VO;

	if (!frame_is_data_qos(frame))
		return IEEE80211_AC_BE;

	p = frame_get_qos_ctl(frame);
	priority = *p & QOS_CTL_TAG1D_MASK;

	return ieee802_1d_to_ac[priority];
}

static double dBm_to_milliwatt(int decibel_intf)
{
#define INTF_LIMIT (31)
	int intf_diff = NOISE_LEVEL - decibel_intf;

	if (intf_diff >= INTF_LIMIT)
		return 0.001;

	if (intf_diff <= -INTF_LIMIT)
		return 1000.0;

	return pow(10.0, -intf_diff / 10.0);
}

static double milliwatt_to_dBm(double value)
{
	return 10.0 * log10(value);
}

static int set_interference_duration(struct wmediumd *ctx, int src_idx,
				     int duration, int signal)
{
	int i;

	if (!ctx->intf)
		return 0;

	if (signal >= CCA_THRESHOLD)
		return 0;

	for (i = 0; i < ctx->num_stas; i++) {
		ctx->intf[ctx->num_stas * src_idx + i].duration += duration;
		// use only latest value
		ctx->intf[ctx->num_stas * src_idx + i].signal = signal;
	}

	return 1;
}

static int get_signal_offset_by_interference(struct wmediumd *ctx, int src_idx,
					     int dst_idx)
{
	int i;
	double intf_power;

	if (!ctx->intf)
		return 0;

	intf_power = 0.0;
	for (i = 0; i < ctx->num_stas; i++) {
		if (i == src_idx || i == dst_idx)
			continue;
		if (drand48() < ctx->intf[i * ctx->num_stas + dst_idx].prob_col)
			intf_power += dBm_to_milliwatt(
				ctx->intf[i * ctx->num_stas + dst_idx].signal);
	}

	if (intf_power <= 1.0)
		return 0;

	return (int)(milliwatt_to_dBm(intf_power) + 0.5);
}

bool is_multicast_ether_addr(const u8 *addr)
{
	return 0x01 & addr[0];
}

static struct station *get_station_by_addr(struct wmediumd *ctx, u8 *addr)
{
	struct station *station;

	list_for_each_entry(station, &ctx->stations, list) {
		if (memcmp(station->addr, addr, ETH_ALEN) == 0)
			return station;
	}
	return NULL;
}

void queue_frame(struct wmediumd *ctx, struct station *station,
		 struct frame *frame)
{
	struct ieee80211_hdr *hdr = (void *)frame->data;
	u8 *dest = hdr->addr1;
	uint64_t target;
	struct wqueue *queue;
	struct frame *tail;
	struct station *tmpsta, *deststa;
	int send_time;
	int cw;
	double error_prob;
	bool is_acked = false;
	bool noack = false;
	int i, j;
	int rate_idx;
	int ac;

	/* TODO configure phy parameters */
	int slot_time = 9;
	int sifs = 16;
	int difs = 2 * slot_time + sifs;

	int retries = 0;

	int ack_time_usec = pkt_duration(14, index_to_rate(0, frame->freq)) +
	                    sifs;

	/*
	 * To determine a frame's expiration time, we compute the
	 * number of retries we might have to make due to radio conditions
	 * or contention, and add backoff time accordingly.  To that, we
	 * add the expiration time of the previous frame in the queue.
	 */

	ac = frame_select_queue_80211(frame);
	queue = &station->queues[ac];

	/* try to "send" this frame at each of the rates in the rateset */
	send_time = 0;
	cw = queue->cw_min;

	int snr = SNR_DEFAULT;

	if (is_multicast_ether_addr(dest)) {
		deststa = NULL;
	} else {
		deststa = get_station_by_addr(ctx, dest);
		if (deststa) {
			snr = ctx->get_link_snr(ctx, station, deststa) -
				get_signal_offset_by_interference(ctx,
					station->index, deststa->index);
			snr += ctx->get_fading_signal(ctx);
		}
	}
	frame->signal = snr + NOISE_LEVEL;

	noack = frame_is_mgmt(frame) || is_multicast_ether_addr(dest);

	double choice = drand48();

	for (i = 0; i < frame->tx_rates_count && !is_acked; i++) {

		rate_idx = frame->tx_rates[i].idx;

		/* no more rates in MRR */
		if (rate_idx < 0)
			break;

		error_prob = ctx->get_error_prob(ctx, snr, rate_idx,
						 frame->freq, frame->data_len,
						 station, deststa);
		for (j = 0; j < frame->tx_rates[i].count; j++) {
			send_time += difs + pkt_duration(frame->data_len,
				index_to_rate(rate_idx, frame->freq));

			retries++;

			/* skip ack/backoff/retries for noack frames */
			if (noack) {
				is_acked = true;
				break;
			}

			/* TODO TXOPs */

			/* backoff */
			if (j > 0) {
				send_time += (cw * slot_time) / 2;
				cw = (cw << 1) + 1;
				if (cw > queue->cw_max)
					cw = queue->cw_max;
			}

			send_time += ack_time_usec;

			if (choice > error_prob) {
				is_acked = true;
				break;
			}

			if (!use_fixed_random_value(ctx))
				choice = drand48();
		}
	}

	if (is_acked) {
		frame->tx_rates[i-1].count = j + 1;
		for (; i < frame->tx_rates_count; i++) {
			frame->tx_rates[i].idx = -1;
			frame->tx_rates[i].count = -1;
		}
		frame->flags |= HWSIM_TX_STAT_ACK;
	}

	/*
	 * delivery time starts after any equal or higher prio frame
	 * (or now, if none).
	 */
	target = scheduler.current_time;
	for (i = 0; i <= ac; i++) {
		list_for_each_entry(tmpsta, &ctx->stations, list) {
			tail = list_last_entry_or_null(&tmpsta->queues[i].frames,
						       struct frame, list);
			if (tail && target < tail->job.start)
				target = tail->job.start;
		}
	}

	target += send_time;

	frame->duration = send_time;
	frame->dest = deststa ? deststa->client : NULL;
	frame->src = station->client;
	frame->job.start = target;
	frame->job.callback = wmediumd_deliver_frame;
	frame->job.data = ctx;
	frame->job.name = "frame";
	usfstl_sched_add_job(&scheduler, &frame->job);
	list_add_tail(&frame->list, &queue->frames);
}

static void wmediumd_send_to_client(struct wmediumd *ctx,
				    struct client *client,
				    struct nl_msg *msg)
{
	size_t len;
	int ret;

	switch (client->type) {
	case CLIENT_NETLINK:
		ret = nl_send_auto_complete(ctx->sock, msg);
		if (ret < 0)
			w_logf(ctx, LOG_ERR, "%s: nl_send_auto failed\n", __func__);
		break;
	case CLIENT_VHOST_USER:
		len = nlmsg_total_size(nlmsg_datalen(nlmsg_hdr(msg)));
		usfstl_vhost_user_dev_notify(client->dev, HWSIM_VQ_RX,
					     (void *)nlmsg_hdr(msg), len);
		break;
	}
}

/*
 * Report transmit status to the kernel.
 */
static void send_tx_info_frame_nl(struct wmediumd *ctx, struct frame *frame)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg) {
		w_logf(ctx, LOG_ERR, "Error allocating new message MSG!\n");
		return;
	}

	if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, ctx->family_id,
			0, NLM_F_REQUEST, HWSIM_CMD_TX_INFO_FRAME,
			VERSION_NR) == NULL) {
		w_logf(ctx, LOG_ERR, "%s: genlmsg_put failed\n", __func__);
		goto out;
	}

	if (nla_put(msg, HWSIM_ATTR_ADDR_TRANSMITTER, ETH_ALEN,
		    frame->sender->hwaddr) ||
	    nla_put_u32(msg, HWSIM_ATTR_FLAGS, frame->flags) ||
	    nla_put_u32(msg, HWSIM_ATTR_SIGNAL, frame->signal) ||
	    nla_put(msg, HWSIM_ATTR_TX_INFO,
		    frame->tx_rates_count * sizeof(struct hwsim_tx_rate),
		    frame->tx_rates) ||
	    nla_put_u64(msg, HWSIM_ATTR_COOKIE, frame->cookie)) {
		w_logf(ctx, LOG_ERR, "%s: Failed to fill a payload\n", __func__);
		goto out;
	}

	wmediumd_send_to_client(ctx, frame->src, msg);

out:
	nlmsg_free(msg);
}

/*
 * Send a data frame to the kernel for reception at a specific radio.
 */
static void send_cloned_frame_msg(struct wmediumd *ctx, struct station *dst,
				  u8 *data, int data_len, int rate_idx,
				  int signal, int freq)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg) {
		w_logf(ctx, LOG_ERR, "Error allocating new message MSG!\n");
		return;
	}

	if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, ctx->family_id,
			0, NLM_F_REQUEST, HWSIM_CMD_FRAME,
			VERSION_NR) == NULL) {
		w_logf(ctx, LOG_ERR, "%s: genlmsg_put failed\n", __func__);
		goto out;
	}

	if (nla_put(msg, HWSIM_ATTR_ADDR_RECEIVER, ETH_ALEN,
		    dst->hwaddr) ||
	    nla_put(msg, HWSIM_ATTR_FRAME, data_len, data) ||
	    nla_put_u32(msg, HWSIM_ATTR_RX_RATE, 1) ||
	    nla_put_u32(msg, HWSIM_ATTR_FREQ, freq) ||
	    nla_put_u32(msg, HWSIM_ATTR_SIGNAL, -50)) {
		w_logf(ctx, LOG_ERR, "%s: Failed to fill a payload\n", __func__);
		goto out;
	}

	w_logf(ctx, LOG_DEBUG, "cloned msg dest " MAC_FMT " (radio: " MAC_FMT ") len %d\n",
		   MAC_ARGS(dst->addr), MAC_ARGS(dst->hwaddr), data_len);

	if (dst->client) {
		wmediumd_send_to_client(ctx, dst->client, msg);
	} else {
		struct client *client;

		list_for_each_entry(client, &ctx->clients, list)
			wmediumd_send_to_client(ctx, client, msg);
	}

out:
	nlmsg_free(msg);
}

void wmediumd_deliver_frame(struct usfstl_job *job)
{
	struct wmediumd *ctx = job->data;
	struct frame *frame = container_of(job, struct frame, job);
	struct ieee80211_hdr *hdr = (void *) frame->data;
	struct station *receiver;
	u8 *dest = hdr->addr1;
	u8 *src = frame->sender->addr;

	list_del(&frame->list);

	if (frame->flags & HWSIM_TX_STAT_ACK) {
		/* rx the frame on the dest interface */
		list_for_each_entry(receiver, &ctx->stations, list) {
			int snr = 0, signal;

			if (memcmp(src, receiver->addr, ETH_ALEN) == 0)
				continue;

			if (memcmp(dest, receiver->addr, ETH_ALEN) == 0) {
				signal = frame->signal;
			} else if (is_multicast_ether_addr(dest)) {
				snr = ctx->get_link_snr(ctx, frame->sender,
							receiver);
				snr += ctx->get_fading_signal(ctx);
				signal = snr + NOISE_LEVEL;
			} else
				continue;

			if (set_interference_duration(ctx,
				frame->sender->index, frame->duration,
				signal))
				continue;

			if (is_multicast_ether_addr(dest)) {
				int rate_idx;
				double error_prob;

				/*
				 * we may or may not receive this based on
				 * reverse link from sender -- check for
				 * each receiver.
				 */
				if (signal < CCA_THRESHOLD)
					continue;

				snr -= get_signal_offset_by_interference(ctx,
					frame->sender->index, receiver->index);
				rate_idx = frame->tx_rates[0].idx;
				error_prob = ctx->get_error_prob(ctx,
					(double)snr, rate_idx, frame->freq,
					frame->data_len, frame->sender,
					receiver);

				if (drand48() <= error_prob) {
					w_logf(ctx, LOG_INFO, "Dropped mcast from "
						   MAC_FMT " to " MAC_FMT " at receiver\n",
						   MAC_ARGS(src), MAC_ARGS(receiver->addr));
					continue;
				}
			}

			send_cloned_frame_msg(ctx, receiver, frame->data,
					      frame->data_len, 1, signal,
					      frame->freq);
		}
	} else
		set_interference_duration(ctx, frame->sender->index,
					  frame->duration, frame->signal);

	send_tx_info_frame_nl(ctx, frame);

	free(frame);
}

void wmediumd_intf_update(struct usfstl_job *job)
{
	struct wmediumd *ctx = job->data;
	int i, j;

	for (i = 0; i < ctx->num_stas; i++)
		for (j = 0; j < ctx->num_stas; j++) {
			if (i == j)
				continue;
			// probability is used for next calc
			ctx->intf[i * ctx->num_stas + j].prob_col =
				ctx->intf[i * ctx->num_stas + j].duration /
				(double)10000;
			ctx->intf[i * ctx->num_stas + j].duration = 0;
		}

	job->start += 10000;
	usfstl_sched_add_job(&scheduler, job);
}

static
int nl_err_cb(struct sockaddr_nl *nla, struct nlmsgerr *nlerr, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(&nlerr->msg);
	struct wmediumd *ctx = arg;

	w_flogf(ctx, LOG_ERR, stderr, "nl: cmd %d, seq %d: %s\n", gnlh->cmd,
			nlerr->msg.nlmsg_seq, strerror(abs(nlerr->error)));

	return NL_SKIP;
}

/*
 * Handle events from the kernel.  Process CMD_FRAME events and queue them
 * for later delivery with the scheduler.
 */
static void _process_messages(struct nl_msg *msg,
			      struct wmediumd *ctx,
			      struct client *client)
{
	struct nlattr *attrs[HWSIM_ATTR_MAX+1];
	/* netlink header */
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	/* generic netlink header*/
	struct genlmsghdr *gnlh = nlmsg_data(nlh);

	struct station *sender;
	struct frame *frame;
	struct ieee80211_hdr *hdr;
	u8 *src;

	if (gnlh->cmd == HWSIM_CMD_FRAME) {
		/* we get the attributes*/
		genlmsg_parse(nlh, 0, attrs, HWSIM_ATTR_MAX, NULL);
		if (attrs[HWSIM_ATTR_ADDR_TRANSMITTER]) {
			u8 *hwaddr = (u8 *)nla_data(attrs[HWSIM_ATTR_ADDR_TRANSMITTER]);

			unsigned int data_len =
				nla_len(attrs[HWSIM_ATTR_FRAME]);
			char *data = (char *)nla_data(attrs[HWSIM_ATTR_FRAME]);
			unsigned int flags =
				nla_get_u32(attrs[HWSIM_ATTR_FLAGS]);
			unsigned int tx_rates_len =
				nla_len(attrs[HWSIM_ATTR_TX_INFO]);
			struct hwsim_tx_rate *tx_rates =
				(struct hwsim_tx_rate *)
				nla_data(attrs[HWSIM_ATTR_TX_INFO]);
			u64 cookie = nla_get_u64(attrs[HWSIM_ATTR_COOKIE]);
			u32 freq;

			freq = attrs[HWSIM_ATTR_FREQ] ?
				nla_get_u32(attrs[HWSIM_ATTR_FREQ]) : 2412;

			hdr = (struct ieee80211_hdr *)data;
			src = hdr->addr2;

			if (data_len < 6 + 6 + 4)
				return;

			sender = get_station_by_addr(ctx, src);
			if (!sender) {
				w_flogf(ctx, LOG_ERR, stderr, "Unable to find sender station " MAC_FMT "\n", MAC_ARGS(src));
				return;
			}
			memcpy(sender->hwaddr, hwaddr, ETH_ALEN);
			if (!sender->client)
				sender->client = client;

			frame = calloc(1, sizeof(*frame) + data_len);
			if (!frame)
				return;

			memcpy(frame->data, data, data_len);
			frame->data_len = data_len;
			frame->flags = flags;
			frame->cookie = cookie;
			frame->freq = freq;
			frame->sender = sender;
			frame->tx_rates_count =
				tx_rates_len / sizeof(struct hwsim_tx_rate);
			memcpy(frame->tx_rates, tx_rates,
			       min(tx_rates_len, sizeof(frame->tx_rates)));
			queue_frame(ctx, sender, frame);
		}
	}
}

static int process_messages_cb(struct nl_msg *msg, void *arg)
{
	struct wmediumd *ctx = arg;

	_process_messages(msg, ctx, &ctx->nl_client);
	return 0;
}

static void wmediumd_vu_connected(struct usfstl_vhost_user_dev *dev)
{
	struct wmediumd *ctx = dev->server->data;
	struct client *client;

	client = calloc(1, sizeof(*client));
	dev->data = client;
	client->type = CLIENT_VHOST_USER;
	client->dev = dev;
	list_add(&client->list, &ctx->clients);
}

static void wmediumd_vu_handle(struct usfstl_vhost_user_dev *dev,
			       struct usfstl_vhost_user_buf *buf,
			       unsigned int vring)
{
	struct nl_msg *nlmsg;
	char data[4096];
	size_t len;

	len = iov_read(data, sizeof(data), buf->out_sg, buf->n_out_sg);

	if (!nlmsg_ok((const struct nlmsghdr *)data, len))
		return;
	nlmsg = nlmsg_convert((struct nlmsghdr *)data);
	if (!nlmsg)
		return;

	_process_messages(nlmsg, dev->server->data, dev->data);

	nlmsg_free(nlmsg);
}

static void wmediumd_vu_disconnected(struct usfstl_vhost_user_dev *dev)
{
	struct client *client = dev->data;

	dev->data = NULL;
	list_del(&client->list);
	free(client);
}

static const struct usfstl_vhost_user_ops wmediumd_vu_ops = {
	.connected = wmediumd_vu_connected,
	.handle = wmediumd_vu_handle,
	.disconnected = wmediumd_vu_disconnected,
};

/*
 * Register with the kernel to start receiving new frames.
 */
int send_register_msg(struct wmediumd *ctx)
{
	struct nl_sock *sock = ctx->sock;
	struct nl_msg *msg;
	int ret;

	msg = nlmsg_alloc();
	if (!msg) {
		w_logf(ctx, LOG_ERR, "Error allocating new message MSG!\n");
		return -1;
	}

	if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, ctx->family_id,
			0, NLM_F_REQUEST, HWSIM_CMD_REGISTER,
			VERSION_NR) == NULL) {
		w_logf(ctx, LOG_ERR, "%s: genlmsg_put failed\n", __func__);
		ret = -1;
		goto out;
	}

	ret = nl_send_auto_complete(sock, msg);
	if (ret < 0) {
		w_logf(ctx, LOG_ERR, "%s: nl_send_auto failed\n", __func__);
		ret = -1;
		goto out;
	}
	ret = 0;

out:
	nlmsg_free(msg);
	return ret;
}

static void sock_event_cb(struct usfstl_loop_entry *entry)
{
	struct wmediumd *ctx = entry->data;

	nl_recvmsgs_default(ctx->sock);
}

/*
 * Setup netlink socket and callbacks.
 */
static int init_netlink(struct wmediumd *ctx)
{
	struct nl_sock *sock;
	int ret;

	ctx->cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!ctx->cb) {
		w_logf(ctx, LOG_ERR, "Error allocating netlink callbacks\n");
		return -1;
	}

	sock = nl_socket_alloc_cb(ctx->cb);
	if (!sock) {
		w_logf(ctx, LOG_ERR, "Error allocating netlink socket\n");
		return -1;
	}

	ctx->sock = sock;

	ret = genl_connect(sock);
	if (ret < 0) {
		w_logf(ctx, LOG_ERR, "Error connecting netlink socket ret=%d\n", ret);
		return -1;
	}

	ctx->family_id = genl_ctrl_resolve(sock, "MAC80211_HWSIM");
	if (ctx->family_id < 0) {
		w_logf(ctx, LOG_ERR, "Family MAC80211_HWSIM not registered\n");
		return -1;
	}

	nl_cb_set(ctx->cb, NL_CB_MSG_IN, NL_CB_CUSTOM, process_messages_cb, ctx);
	nl_cb_err(ctx->cb, NL_CB_CUSTOM, nl_err_cb, ctx);

	return 0;
}

/*
 *	Print the CLI help
 */
void print_help(int exval)
{
	printf("wmediumd v%s - a wireless medium simulator\n", VERSION_STR);
	printf("wmediumd [-h] [-V] [-l LOG_LVL] [-x FILE] -c FILE \n\n");

	printf("  -h              print this help and exit\n");
	printf("  -V              print version and exit\n\n");

	printf("  -l LOG_LVL      set the logging level\n");
	printf("                  LOG_LVL: RFC 5424 severity, values 0 - 7\n");
	printf("                  >= 3: errors are logged\n");
	printf("                  >= 5: startup msgs are logged\n");
	printf("                  >= 6: dropped packets are logged (default)\n");
	printf("                  == 7: all packets will be logged\n");
	printf("  -c FILE         set input config file\n");
	printf("  -x FILE         set input PER file\n");
	printf("  -t socket       set the time control socket\n");
	printf("  -u socket       expose vhost-user socket, don't use netlink\n");
	printf("  -n              force netlink use even with vhost-user\n");

	exit(exval);
}

int main(int argc, char *argv[])
{
	int opt;
	struct wmediumd ctx = {};
	char *config_file = NULL;
	char *per_file = NULL;
	const char *time_socket = NULL;
	struct usfstl_sched_ctrl ctrl = {};
	struct usfstl_vhost_user_server vusrv = {
		.ops = &wmediumd_vu_ops,
		.max_queues = HWSIM_NUM_VQS,
		.input_queues = 1 << HWSIM_VQ_TX,
		.protocol_features =
			1ULL << VHOST_USER_PROTOCOL_F_INBAND_NOTIFICATIONS,
		.data = &ctx,
	};
	bool use_netlink, force_netlink = false;

	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

	if (argc == 1) {
		fprintf(stderr, "This program needs arguments....\n\n");
		print_help(EXIT_FAILURE);
	}

	ctx.log_lvl = 6;
	unsigned long int parse_log_lvl;
	char* parse_end_token;

	while ((opt = getopt(argc, argv, "hVc:l:x:t:u:n")) != -1) {
		switch (opt) {
		case 'h':
			print_help(EXIT_SUCCESS);
			break;
		case 'V':
			printf("wmediumd v%s - a wireless medium simulator "
			       "for mac80211_hwsim\n", VERSION_STR);
			exit(EXIT_SUCCESS);
			break;
		case 'c':
			config_file = optarg;
			break;
		case 'x':
			printf("Input packet error rate file: %s\n", optarg);
			per_file = optarg;
			break;
		case ':':
			printf("wmediumd: Error - Option `%c' "
			       "needs a value\n\n", optopt);
			print_help(EXIT_FAILURE);
			break;
		case 'l':
			parse_log_lvl = strtoul(optarg, &parse_end_token, 10);
			if ((parse_log_lvl == ULONG_MAX && errno == ERANGE) ||
			     optarg == parse_end_token || parse_log_lvl > 7) {
				printf("wmediumd: Error - Invalid RFC 5424 severity level: "
							   "%s\n\n", optarg);
				print_help(EXIT_FAILURE);
			}
			ctx.log_lvl = parse_log_lvl;
			break;
		case 't':
			time_socket = optarg;
			break;
		case 'u':
			vusrv.socket = optarg;
			break;
		case 'n':
			force_netlink = true;
			break;
		case '?':
			printf("wmediumd: Error - No such option: "
			       "`%c'\n\n", optopt);
			print_help(EXIT_FAILURE);
			break;
		}

	}

	if (optind < argc)
		print_help(EXIT_FAILURE);

	if (!config_file) {
		printf("%s: config file must be supplied\n", argv[0]);
		print_help(EXIT_FAILURE);
	}

	w_logf(&ctx, LOG_NOTICE, "Input configuration file: %s\n", config_file);

	INIT_LIST_HEAD(&ctx.stations);
	INIT_LIST_HEAD(&ctx.clients);

	if (load_config(&ctx, config_file, per_file))
		return EXIT_FAILURE;

	use_netlink = force_netlink || !vusrv.socket;

	/* init netlink */
	if (use_netlink && init_netlink(&ctx) < 0)
		return EXIT_FAILURE;

	if (ctx.intf) {
		ctx.intf_job.start = 10000; // usec
		ctx.intf_job.name = "interference update";
		ctx.intf_job.data = &ctx;
		ctx.intf_job.callback = wmediumd_intf_update;
		usfstl_sched_add_job(&scheduler, &ctx.intf_job);
	}

	if (time_socket) {
		usfstl_sched_ctrl_start(&ctrl, time_socket,
				      1000 /* nsec per usec */,
				      (uint64_t)-1 /* no ID */,
				      &scheduler);
		vusrv.scheduler = &scheduler;
		vusrv.ctrl = &ctrl;
	} else {
		usfstl_sched_wallclock_init(&scheduler, 1000);
	}

	if (vusrv.socket)
		usfstl_vhost_user_server_start(&vusrv);

	if (use_netlink) {
		ctx.nl_client.type = CLIENT_NETLINK;
		list_add(&ctx.nl_client.list, &ctx.clients);

		ctx.nl_loop.handler = sock_event_cb;
		ctx.nl_loop.data = &ctx;
		ctx.nl_loop.fd = nl_socket_get_fd(ctx.sock);
		usfstl_loop_register(&ctx.nl_loop);

		/* register for new frames */
		if (send_register_msg(&ctx) == 0)
			w_logf(&ctx, LOG_NOTICE, "REGISTER SENT!\n");
	}

	while (1) {
		if (time_socket) {
			usfstl_sched_next(&scheduler);
		} else {
			usfstl_sched_wallclock_wait_and_handle(&scheduler);

			if (usfstl_sched_next_pending(&scheduler, NULL))
				usfstl_sched_next(&scheduler);
		}
	}

	free(ctx.sock);
	free(ctx.cb);
	free(ctx.intf);
	free(ctx.per_matrix);

	return EXIT_SUCCESS;
}
