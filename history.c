#include <assert.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "curl/url.h"
#include "history.h"
#include "gemini.h"

#define MAXHISTSZ 4096

size_t histpos = 0;
struct Gemdoc *history[MAXHISTSZ];

static struct Gemdoc *
_hist_at(size_t pos)
{
	if (pos < MAXHISTSZ && history[pos]) {
		histpos = pos;
		return history[histpos];
	} else {
		return NULL;
	}
}

void
hist_init(void)
{
	for (size_t i = 0; i < MAXHISTSZ; ++i)
		history[i] = NULL;
}

size_t
hist_len(void)
{
	for (size_t i = 0; i < MAXHISTSZ; ++i)
		if (!history[i]) return i;
	return MAXHISTSZ;
}

size_t
hist_contains(CURLU *url)
{
	char *a_url, *b_url;
	curl_url_get(url, CURLUPART_URL, &a_url, 0);
	size_t found = 0;

	for (size_t i = 0; i < hist_len(); ++i) {
		curl_url_get(history[i]->url, CURLUPART_URL, &b_url, 0);
		if (b_url)
			if (!strcmp(a_url, b_url)) ++found;
		free(b_url);
	}

	free(a_url);
	return found;
}

void
hist_add(struct Gemdoc *g)
{
	assert(g);

	if ((histpos+1) >= hist_len()) {
		history[histpos] = g;
		++histpos;
	} else {
		for (size_t i = ++histpos; i < MAXHISTSZ; ++i) {
			if (history[i])
				gemdoc_free(history[i]);
			history[i] = NULL;
		}
		history[histpos] = g;
	}
}

struct Gemdoc *
hist_back(void)
{
	return _hist_at(histpos - 1);
}

struct Gemdoc *
hist_forw(void)
{
	return _hist_at(histpos + 1);
}

void
hist_free(void)
{
	for (size_t i = 0; i < MAXHISTSZ; ++i) {
		gemdoc_free(history[i]);
		history[i] = NULL;
	}
}
