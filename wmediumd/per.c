/*
 * Generate packet error rates for OFDM rates given signal level and
 * packet length.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wmediumd.h"

static double get_error_prob_from_per_matrix(struct wmediumd *ctx, double snr,
					     unsigned int rate_idx,
					     int frame_len, struct station *src,
					     struct station *dst)
{
	int signal_idx;

	signal_idx = snr + NOISE_LEVEL - ctx->per_matrix_signal_min;

	if (signal_idx < 0)
		return 1.0;

	if (signal_idx >= ctx->per_matrix_row_num)
		return 0.0;

	if (rate_idx >= PER_MATRIX_RATE_LEN) {
		w_flogf(ctx, LOG_WARNING, stderr,
			"%s: invalid rate_idx=%d is replaced by %d\n",
			__func__, rate_idx, PER_MATRIX_RATE_LEN - 1);
		rate_idx = PER_MATRIX_RATE_LEN - 1;
	}

	return ctx->per_matrix[signal_idx * PER_MATRIX_RATE_LEN + rate_idx];
}

int set_default_per(struct wmediumd *ctx)
{
	/* Default value is same as tests/signal_table_ieee80211ax file. */
	float default_per_matrix[] = {
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.9995, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.529, 0.9995, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.0427, 0.529, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.0014, 0.0427, 0.9997, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.0014, 0.5462, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.0439, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.0016, 0.9597, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.2239, 1.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.0117, 0.8908, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.2343, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.024, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 1.00E+00, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.979, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.3536, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.0356, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.0018, 1.00E+00, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.9496, 1.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.379, 0.9981,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.061, 0.6465,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.0057, 0.1343,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.0004, 0.0145,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.0007,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00,
		0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00, 0.00E+00
	};

	ctx->per_matrix = malloc(sizeof(default_per_matrix));
	if (ctx->per_matrix == NULL)
		return EXIT_FAILURE;

	memcpy(ctx->per_matrix, default_per_matrix, sizeof(default_per_matrix));
	ctx->per_matrix_signal_min = -100;
	ctx->per_matrix_row_num = sizeof(default_per_matrix) / sizeof(float) /
					PER_MATRIX_RATE_LEN;
	ctx->get_error_prob = get_error_prob_from_per_matrix;

	return EXIT_SUCCESS;
}

int read_per_file(struct wmediumd *ctx, const char *file_name)
{
	FILE *fp;
	char line[256];
	int signal, i;
	float *temp;

	fp = fopen(file_name, "r");
	if (fp == NULL) {
		w_flogf(ctx, LOG_ERR, stderr,
			"fopen failed %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	ctx->per_matrix_signal_min = 1000;
	while (fscanf(fp, "%s", line) != EOF){
		if (line[0] == '#') {
			if (fgets(line, sizeof(line), fp) == NULL) {
				w_flogf(ctx, LOG_ERR, stderr,
					"Failed to read comment line\n");
				return EXIT_FAILURE;
			}
			continue;
		}

		signal = atoi(line);
		if (ctx->per_matrix_signal_min > signal)
			ctx->per_matrix_signal_min = signal;

		if (signal - ctx->per_matrix_signal_min < 0) {
			w_flogf(ctx, LOG_ERR, stderr,
				"%s: invalid signal=%d\n", __func__, signal);
			return EXIT_FAILURE;
		}

		temp = realloc(ctx->per_matrix, sizeof(float) *
				PER_MATRIX_RATE_LEN *
				++ctx->per_matrix_row_num);
		if (temp == NULL) {
			w_flogf(ctx, LOG_ERR, stderr,
				"Out of memory(PER file)\n");
			return EXIT_FAILURE;
		}
		ctx->per_matrix = temp;

		for (i = 0; i < PER_MATRIX_RATE_LEN; i++) {
			if (fscanf(fp, "%f", &ctx->per_matrix[
				(signal - ctx->per_matrix_signal_min) *
				PER_MATRIX_RATE_LEN + i]) == EOF) {
				w_flogf(ctx, LOG_ERR, stderr,
					"Not enough rate found\n");
				return EXIT_FAILURE;
			}
		}
	}

	ctx->get_error_prob = get_error_prob_from_per_matrix;

	return EXIT_SUCCESS;
}
