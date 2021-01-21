#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "curl/url.h"
#include "history.h"
#include "gemini.h"
#include "util.h"

static struct Gemdoc *
_hist_at(struct History *h, size_t pos)
{
	if (pos < MAXHISTSZ && h->visited[pos]) {
		h->current = pos;
		return h->visited[h->current];
	} else {
		return NULL;
	}
}

void
hist_init(struct History *h)
{
	for (size_t i = 0; i < MAXHISTSZ; ++i)
		h->visited[i] = NULL;
}

size_t
hist_len(struct History *h)
{
	for (size_t i = 0; i < MAXHISTSZ; ++i)
		if (!h->visited[i]) return i;
	return MAXHISTSZ;
}

size_t
hist_contains(struct History *h, CURLU *url)
{
	char *a_url, *b_url;
	curl_url_get(url, CURLUPART_URL, &a_url, 0);
	size_t found = 0;

	for (size_t i = 0; i < hist_len(h); ++i) {
		curl_url_get(h->visited[i]->url, CURLUPART_URL, &b_url, 0);
		if (b_url)
			if (!strcmp(a_url, b_url)) ++found;
		free(b_url);
	}

	free(a_url);
	return found;
}

void
hist_add(struct History *h, struct Gemdoc *g)
{
	ENSURE(g);

	if ((h->current+1) >= hist_len(h)) {
		h->visited[h->current] = g;
		++h->current;
	} else {
		for (size_t i = ++h->current; i < MAXHISTSZ; ++i) {
			if (h->visited[i])
				gemdoc_free(h->visited[i]);
			h->visited[i] = NULL;
		}
		h->visited[h->current] = g;
	}
}

struct Gemdoc *
hist_back(struct History *h)
{
	return _hist_at(h, h->current - 1);
}

struct Gemdoc *
hist_forw(struct History *h)
{
	return _hist_at(h, h->current + 1);
}

void
hist_free(struct History *h)
{
	for (size_t i = 0; i < MAXHISTSZ; ++i) {
		gemdoc_free(h->visited[i]);
		h->visited[i] = NULL;
	}
}
