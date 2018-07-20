#ifndef _FILTER_H
#define _FILTER_H

#include "wmediumd.h"

struct frame;

enum filter_options {
	FILTER_PASS,
	FILTER_DROP
};

enum filter_frame_types {
	FILTER_TYPE_NONE,
	FILTER_TYPE_COMMIT,
	FILTER_TYPE_CONFIRM,
	FILTER_TYPE_ACTION,
};

struct filter {
	uint8_t mac[ETH_ALEN];
	enum filter_frame_types frame_type;
	int count;
};

struct filter *filter_parse(char *filter_str);
void filter_destroy(struct filter *filter);
enum filter_options filter_matches(struct filter *filter, struct frame *frame);

#endif
