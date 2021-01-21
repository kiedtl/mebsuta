#ifndef HISTORY_H
#define HISTORY_H

#include <sys/types.h>
#include "curl/url.h"
#include "gemini.h"

#define MAXHISTSZ 4096

struct History {
	struct Gemdoc *visited[MAXHISTSZ];
	size_t current;
};

void hist_init(struct History *h);
size_t hist_len(struct History *h);
size_t hist_contains(struct History *h, CURLU *url);
void hist_add(struct History *h, struct Gemdoc *g);
struct Gemdoc *hist_back(struct History *h);
struct Gemdoc *hist_forw(struct History *h);
void hist_free(struct History *h);

#endif
