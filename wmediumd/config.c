/*
 *	wmediumd, wireless medium simulator for mac80211_hwsim kernel module
 *	Copyright (c) 2011 cozybit Inc.
 *
 *	Author: Javier Lopez    <jlopex@cozybit.com>
 *		Javier Cardona  <javier@cozybit.com>
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

#include <libconfig.h>
#include <string.h>
#include <stdlib.h>

#include "wmediumd.h"

static void string_to_mac_address(const char *str, u8 *addr)
{
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

static int get_link_snr_default(struct wmediumd *ctx, struct station *sender,
				 struct station *receiver)
{
	return SNR_DEFAULT;
}

static int get_link_snr_from_snr_matrix(struct wmediumd *ctx,
					struct station *sender,
					struct station *receiver)
{
	return ctx->snr_matrix[sender->index * ctx->num_stas + receiver->index];
}

static double _get_error_prob_from_snr(struct wmediumd *ctx, double snr,
				       unsigned int rate_idx, int frame_len,
				       struct station *src, struct station *dst)
{
	return get_error_prob_from_snr(snr, rate_idx, frame_len);
}

static double get_error_prob_from_matrix(struct wmediumd *ctx, double snr,
					 unsigned int rate_idx, int frame_len,
					 struct station *src,
					 struct station *dst)
{
	if (dst == NULL) // dst is multicast. returned value will not be used.
		return 0.0;

	return ctx->error_prob_matrix[ctx->num_stas * src->index + dst->index];
}

int use_fixed_random_value(struct wmediumd *ctx)
{
	return ctx->error_prob_matrix != NULL;
}

/*
 *	Loads a config file into memory
 */
int load_config(struct wmediumd *ctx, const char *file)
{
	config_t cfg, *cf;
	const config_setting_t *ids;
	const config_setting_t *links;
	const config_setting_t *error_probs, *error_prob;
	int count_ids, i;
	int start, end, snr;
	struct station *station;

	/*initialize the config file*/
	cf = &cfg;
	config_init(cf);

	/*read the file*/
	if (!config_read_file(cf, file)) {
		fprintf(stderr, "Error loading file %s at line:%d, reason: %s\n",
		       file,
		       config_error_line(cf),
		       config_error_text(cf));
		config_destroy(cf);
		return EXIT_FAILURE;
	}

	ids = config_lookup(cf, "ifaces.ids");
	if (!ids) {
		fprintf(stderr, "ids not found in config file\n");
		return EXIT_FAILURE;
	}
	count_ids = config_setting_length(ids);

	printf("#_if = %d\n", count_ids);

	/* Fill the mac_addr */
	for (i = 0; i < count_ids; i++) {
		u8 addr[ETH_ALEN];
		const char *str =  config_setting_get_string_elem(ids, i);
		string_to_mac_address(str, addr);

		station = malloc(sizeof(*station));
		if (!station) {
			fprintf(stderr, "Out of memory!\n");
			return EXIT_FAILURE;
		}
		station->index = i;
		memcpy(station->addr, addr, ETH_ALEN);
		memcpy(station->hwaddr, addr, ETH_ALEN);
		station_init_queues(station);
		list_add_tail(&station->list, &ctx->stations);

		printf("Added station %d: " MAC_FMT "\n", i, MAC_ARGS(addr));
	}
	ctx->num_stas = count_ids;

	links = config_lookup(cf, "ifaces.links");
	error_probs = config_lookup(cf, "ifaces.error_probs");

	if (links && error_probs) {
		fprintf(stderr, "specify one of links/error_probs\n");
		goto fail;
	}

	ctx->get_link_snr = get_link_snr_from_snr_matrix;
	ctx->get_error_prob = _get_error_prob_from_snr;

	/* create link quality matrix */
	ctx->snr_matrix = calloc(sizeof(int), count_ids * count_ids);
	if (!ctx->snr_matrix) {
		fprintf(stderr, "Out of memory(snr_matrix)\n");
		return EXIT_FAILURE;
	}

	/* set default snrs */
	for (i = 0; i < count_ids * count_ids; i++)
		ctx->snr_matrix[i] = SNR_DEFAULT;

	ctx->error_prob_matrix = NULL;
	if (error_probs) {
		if (config_setting_length(error_probs) != count_ids) {
			fprintf(stderr, "Specify %d error probabilities\n",
				count_ids);
			goto fail;
		}

		ctx->error_prob_matrix = calloc(sizeof(double),
						count_ids * count_ids);
		if (!ctx->error_prob_matrix) {
			fprintf(stderr, "Out of memory(error_prob_matrix)\n");
			goto fail;
		}

		ctx->get_link_snr = get_link_snr_default;
		ctx->get_error_prob = get_error_prob_from_matrix;
	}

	/* read snr values */
	for (i = 0; links && i < config_setting_length(links); i++) {
		config_setting_t *link;

		link = config_setting_get_elem(links, i);
		if (config_setting_length(link) != 3) {
			fprintf(stderr, "Invalid link: expected (int,int,int)\n");
			continue;
		}
		start = config_setting_get_int_elem(link, 0);
		end = config_setting_get_int_elem(link, 1);
		snr = config_setting_get_int_elem(link, 2);

		if (start < 0 || start >= ctx->num_stas ||
		    end < 0 || end >= ctx->num_stas) {
			fprintf(stderr, "Invalid link [%d,%d,%d]: index out of range\n",
				start, end, snr);
			continue;
		}
		ctx->snr_matrix[ctx->num_stas * start + end] = snr;
		ctx->snr_matrix[ctx->num_stas * end + start] = snr;
	}

	/* read error probabilities */
	for (start = 0; error_probs && start < count_ids; start++) {
		error_prob = config_setting_get_elem(error_probs, start);
		if (config_setting_length(error_prob) != count_ids) {
			fprintf(stderr, "Specify %d error probabilities\n",
				count_ids);
			goto fail;
		}
		for (end = start + 1; end < count_ids; end++) {
			ctx->error_prob_matrix[count_ids * start + end] =
			ctx->error_prob_matrix[count_ids * end + start] =
				config_setting_get_float_elem(error_prob, end);
		}
	}

	config_destroy(cf);
	return EXIT_SUCCESS;

 fail:
	free(ctx->snr_matrix);
	free(ctx->error_prob_matrix);
	config_destroy(cf);
	return EXIT_FAILURE;
}
