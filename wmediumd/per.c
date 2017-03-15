/*
 * Generate packet error rates for OFDM rates given signal level and
 * packet length.
 */

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wmediumd.h"

/* Code rates for convolutional codes */
enum fec_rate {
	FEC_RATE_1_2,
	FEC_RATE_2_3,
	FEC_RATE_3_4,
};

struct rate {
	int mbps;
	int mqam;
	enum fec_rate fec;
};

/* 802.11a rate set */
struct rate rateset[] = {
	{ .mbps = 6, .mqam = 2, .fec = FEC_RATE_1_2 },
	{ .mbps = 9, .mqam = 2, .fec = FEC_RATE_3_4 },
	{ .mbps = 12, .mqam = 4, .fec = FEC_RATE_1_2 },
	{ .mbps = 18, .mqam = 4, .fec = FEC_RATE_3_4 },
	{ .mbps = 24, .mqam = 16, .fec = FEC_RATE_1_2 },
	{ .mbps = 36, .mqam = 16, .fec = FEC_RATE_3_4 },
	{ .mbps = 48, .mqam = 64, .fec = FEC_RATE_2_3 },
	{ .mbps = 54, .mqam = 64, .fec = FEC_RATE_3_4 },
};


#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define PER_MATRIX_RATE_LEN (12)

double n_choose_k(double n, double k)
{
	int i;
	double c = 1;

	if (n < k || !k)
		return 0;

	if (k > n - k)
		k = n - k;

	for (i = 1; i <= k; i++)
		c *= (n - (k - i)) / i;

	return c;
}

double dot(double *v1, double *v2, int len)
{
	int i;
	double val = 0;

	for (i = 0; i < len; i++)
		val += v1[i] * v2[i];

	return val;
}

/*
 * Compute bit error rate for BPSK at a given SNR.
 * See http://en.wikipedia.org/wiki/Phase-shift_keying
 */
double bpsk_ber(double snr_db)
{
	double snr = pow(10, (snr_db / 10.));

	return .5 * erfc(sqrt(snr));
}

/*
 * Compute bit error rate for M-QAM at a given SNR.
 * See http://www.dsplog.com/2012/01/01/symbol-error-rate-16qam-64qam-256qam/
 */
double mqam_ber(int m, double snr_db)
{
	double k = sqrt(1. / ((2./3) * (m - 1)));
	double snr = pow(10, (snr_db / 10.));
	double e = erfc(k * sqrt(snr));
	double sqrtm = sqrt(m);

	double b = 2 * (1 - 1./sqrtm) * e;
	double c = (1 - 2./sqrtm + 1./m) * pow(e, 2);
	double ser = b - c;

	return ser / log2(m);
}

/*
 * Compute packet (frame) error rate given a length
 */
double per(double ber, enum fec_rate rate, int frame_len)
{
	/* free distances for each fec_rate */
	int d_free[] = { 10, 6, 5 };

	/* initial rate code coefficients */
	double a_d[3][10] = {
		/* FEC_RATE_1_2 */
		{ 11, 0, 38, 0, 193, 0, 1331, 0, 7275, 0 },
		/* FEC_RATE_2_3 */
		{ 1, 16, 48, 158, 642, 2435, 9174, 34701, 131533, 499312 },
		/* FEC_RATE_3_4 */
		{ 8, 31, 160, 892, 4512, 23297, 120976, 624304, 3229885, 16721329 }
	};

	double p_d[ARRAY_SIZE(a_d[0])] = {};
	double rho = ber;
	double prob_uncorrected;
	int k;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(p_d); i++) {
		double sum_prob = 0;
		int d = d_free[rate] + i;

		if (d & 1) {
			for (k = (d + 1)/2; k <= d; k++)
				sum_prob += n_choose_k(d, k) * pow(rho, k) *
					    pow(1 - rho, d - k);
		} else {
			for (k = d/2 + 1; k <= d; k++)
				sum_prob += n_choose_k(d, k) * pow(rho, k) *
					    pow(1 - rho, d - k);

			sum_prob += .5 * n_choose_k(d, d/2) * pow(rho, d/2) *
				    pow(1 - rho, d/2);
		}

		p_d[i] = sum_prob;
	}

	prob_uncorrected = dot(p_d, a_d[rate], ARRAY_SIZE(a_d[rate]));
	if (prob_uncorrected > 1)
		prob_uncorrected = 1;

	return 1.0 - pow(1 - prob_uncorrected, 8 * frame_len);
}

double get_error_prob_from_snr(double snr, unsigned int rate_idx, int frame_len)
{
	int m;
	enum fec_rate fec;
	double ber;

	if (snr <= 0.0)
		return 1.0;

	if (rate_idx >= ARRAY_SIZE(rateset))
		return 1.0;

	m = rateset[rate_idx].mqam;
	fec = rateset[rate_idx].fec;

	if (m == 2)
		ber = bpsk_ber(snr);
	else
		ber = mqam_ber(m, snr);

	return per(ber, fec, frame_len);
}

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
