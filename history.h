#ifndef HISTORY_H
#define HISTORY_H

#include <sys/types.h>
#include "curl/url.h"
#include "gemini.h"

#define MAXHISTSZ 4096

void hist_init(struct lnklist **h);
size_t hist_len(struct lnklist *h);
size_t hist_contains(struct lnklist *h, CURLU *url);
void hist_add(struct lnklist **h, struct Gemdoc *g);
void hist_back(struct lnklist **h);
void hist_forw(struct lnklist **h);
void hist_free(struct lnklist *h);

#endif
