#include <string.h>
#include <stdlib.h>
#include "filter.h"
#include "config.h"

struct filter *filter_parse(char *filter_str)
{
	int field_count = 0;
	char *tok;
	char *str = filter_str;
	struct filter filter = {
		.count = -1
	};

	struct filter *ret_filter;

	/* TODO something else here */
	/* possible strings: "[aa:bb:cc:dd:ee:ff].[commit|confirm|action][.count]" */
	while ((tok = strtok(str, "."))) {
		str = NULL;

		switch(field_count++)
		{
		case 0:
			string_to_mac_address(tok, filter.mac);
			break;
		case 1:
			if (!strcmp(tok, "commit"))
				filter.frame_type = FILTER_TYPE_COMMIT;
			else if (!strcmp(tok, "confirm"))
				filter.frame_type = FILTER_TYPE_CONFIRM;
			else if (!strcmp(tok, "action"))
				filter.frame_type = FILTER_TYPE_ACTION;
			else {
				printf("unknown filter type: %s\n", tok);
				return NULL;
			}
			break;
		case 2:
			filter.count = atoi(tok);
			break;
		}
	}

	if (field_count < 2)
		return NULL;

	printf("filtering %d frames of type %d\n", filter.count,
	       filter.frame_type);
	ret_filter = malloc(sizeof(*ret_filter));
	memcpy(ret_filter, &filter, sizeof(filter));
	return ret_filter;
}

void filter_destroy(struct filter *filter)
{
	free(filter);
}

enum filter_options filter_matches(struct filter *filter, struct frame *frame)
{
	if (!filter->count || !filter->frame_type)
		return FILTER_PASS;

	printf(MAC_FMT " [" MAC_FMT "] is_action: %d fc: %x %x\n",
            MAC_ARGS(frame->sender->addr),
	    MAC_ARGS(filter->mac), frame_is_action(frame),
        frame->data[0], frame->data[1]);

	if (memcmp(frame->sender->addr, filter->mac, ETH_ALEN))
		return FILTER_PASS;

	if (filter->frame_type == FILTER_TYPE_COMMIT &&
	    frame_is_sae_commit(frame))
		goto drop;

	if (filter->frame_type == FILTER_TYPE_CONFIRM &&
	    frame_is_sae_confirm(frame))
		goto drop;

	if (filter->frame_type == FILTER_TYPE_ACTION &&
	    frame_is_action(frame))
		goto drop;

	return FILTER_PASS;

drop:
	printf("Dropping frame %d due to filter\n", filter->frame_type);
	if (filter->count > 0)
		--filter->count;
	return FILTER_DROP;
}
