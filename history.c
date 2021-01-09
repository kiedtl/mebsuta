#include <assert.h>
#include <sys/types.h>
#include <stddef.h>
#include <string.h>

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
	for (size_t i = histpos; i < MAXHISTSZ; ++i) {
		gemdoc_free(history[i]);
		history[i] = NULL;
	}
}
