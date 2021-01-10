#ifndef HISTORY_H
#define HISTORY_H

#include <sys/types.h>
#include "curl/url.h"
#include "gemini.h"

extern size_t histpos;
extern struct Gemdoc *history[4096];

void hist_init(void);
size_t hist_len(void);
size_t hist_contains(CURLU *url);
void hist_add(struct Gemdoc *g);
struct Gemdoc *hist_back(void);
struct Gemdoc *hist_forw(void);
void hist_free(void);

#endif
