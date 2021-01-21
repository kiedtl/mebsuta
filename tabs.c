#include <stddef.h>

#include "tabs.h"
#include "util.h"

struct Tab tabs[255];
size_t tab_cur;

void
tabs_init(void)
{
	tab_cur = 0;

	for (size_t i = 0; i < SIZEOF(tabs); ++i) {
		tabs[i].ui_hscroll = tabs[i].ui_vscroll = 0;
		tabs[i].ui_doc_mode = UI_DOCNORM;
		tabs[i].doc = NULL;

		hist_init(&tabs[i].hist);
	}
}

size_t
tabs_len(void)
{
	for (size_t i = 0; i < SIZEOF(tabs); ++i)
		if (tabs[i].doc == NULL)
			return i;
	return SIZEOF(tabs);
}


void
tabs_free(void)
{
	for (size_t i = 0; i < SIZEOF(tabs); ++i) {
		hist_free(&tabs[i].hist);
	}
}
