#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "curl/url.h"
#include "history.h"
#include "gemini.h"
#include "util.h"

void
hist_init(struct lnklist **h)
{
	*h = lnklist_new();
}

size_t
hist_len(struct lnklist *h)
{
	return lnklist_len(h);
}

size_t
hist_contains(struct lnklist *h, CURLU *url)
{
	char *a_url, *b_url;
	curl_url_get(url, CURLUPART_URL, &a_url, 0);
	size_t found = 0;

	for (struct lnklist *l = h->next; l; l = l->next) {
		if (!l->data)
			continue;
		struct Gemdoc *g = (struct Gemdoc *)l->data;
		curl_url_get(g->url, CURLUPART_URL, &b_url, 0);
		if (b_url) {
			if (!strcmp(a_url, b_url)) ++found;
			free(b_url);
		}
	}

	free(a_url);
	return found;
}

void
hist_add(struct lnklist **h, struct Gemdoc *g)
{
	ENSURE(g);

	if ((*h)->next) {
		(*h)->next->prev = NULL;
		lnklist_free((*h)->next);
		(*h)->next = NULL;
		(*h)->data = (void *)g;
	} else {
		lnklist_push((*h), (void *)g);
		(*h) = (*h)->next;
	}
}

void
hist_back(struct lnklist **h)
{
	if ((*h)->prev && (*h)->prev->data) {
		(*h) = (*h)->prev;
	}
}

void
hist_forw(struct lnklist **h)
{
	if ((*h)->next) {
		(*h) = (*h)->next;
	}
}

void
hist_free(struct lnklist *h)
{
	struct lnklist *begin = lnklist_head(h);
	for (struct lnklist *l = begin->next; l; l = l->next) {
		if ((struct Gemdoc *)l->data)
			gemdoc_free((struct Gemdoc *)l->data);
		l->data = NULL;
	}
	ENSURE(lnklist_free(begin));
}
