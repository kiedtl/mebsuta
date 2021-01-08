#ifndef CCOMMON_LIST_H
#define CCOMMON_LIST_H

#include <stdint.h>

struct lnklist {
	void *data;
	struct lnklist *next;
	struct lnklist *prev;
};

/* prototypes */
struct lnklist *lnklist_new(void);
struct lnklist *lnklist_head(struct lnklist *list);
struct lnklist *lnklist_tail(struct lnklist *list);
_Bool  lnklist_push(struct lnklist *list, void *data);
void  *lnklist_pop(struct lnklist *list);
ssize_t lnklist_len(struct lnklist *list);
_Bool  lnklist_free(struct lnklist *list);

#endif
