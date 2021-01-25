#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "list.h"
#include "tabs.h"
#include "util.h"

struct lnklist *tabs;
struct lnklist *curtab;

void
tabs_init(void)
{
	tabs = lnklist_new();
	curtab = tabs;
}

size_t
tabs_len(void)
{
	ssize_t l = lnklist_len(tabs);
	return l >= 0 ? (size_t)l : 0;
}

void
tabs_add(struct lnklist *after)
{
	struct Tab *t = ecalloc(1, sizeof(struct Tab));
	ENSURE(t);
	hist_init(&t->hist);
	ENSURE(lnklist_insert(after, (void *)t));
}

void
tabs_rm(struct lnklist *tab)
{
	ENSURE(tab);
	hist_free(&((struct Tab *)tab->data)->hist);
	free(tab->data);
	ENSURE(lnklist_rm(tab));
}

void
tabs_free(void)
{
	for (struct lnklist *l = tabs->next; l; l = l->next) {
		if (!l->data) continue;

		hist_free(&((struct Tab *)l->data)->hist);
		free(l->data);

		/* Don't free t.doc, since that pointer was present
		 * in t.hist which was freed previously */
	}
	ENSURE(lnklist_free(tabs));
}
