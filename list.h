#ifndef CCOMMON_LIST_H
#define CCOMMON_LIST_H

#include <sys/types.h>

struct lnklist {
	void *data;
	struct lnklist *next;
	struct lnklist *prev;
};

/* prototypes */
struct lnklist *lnklist_new(void);
struct lnklist *lnklist_head(struct lnklist *list);
struct lnklist *lnklist_tail(struct lnklist *list);
_Bool lnklist_push(struct lnklist *list, void *data);
struct lnklist *lnklist_ref(struct lnklist *list, size_t n);
void  *lnklist_pop(struct lnklist *list);
_Bool lnklist_rm(struct lnklist *node);
_Bool lnklist_insert(struct lnklist *after, void *data);
ssize_t lnklist_len(struct lnklist *list);
_Bool lnklist_free(struct lnklist *list);
_Bool lnklist_free_all(struct lnklist *list);

#endif
