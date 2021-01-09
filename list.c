#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#include "list.h"
#include "util.h"

/*
 * allocate list head
 */
struct lnklist *
lnklist_new(void)
{
	struct lnklist *new = calloc(1, sizeof(struct lnklist));
	if (new == NULL) return NULL;
	new->prev = new->next = new->data = NULL;
	return new;
}

/*
 * get first node (head) of list.
 *
 * returns list if it points to list head.
 */
struct lnklist *
lnklist_head(struct lnklist *list)
{
	if (list == NULL) return NULL;

	struct lnklist *head = list, *ctr = list->prev;
	for (; ctr != NULL; head = ctr, ctr = ctr->prev);

	return head;
}

/*
 * get last node of list.
 *
 * returns list if list->next is null
 */
struct lnklist *
lnklist_tail(struct lnklist *list)
{
	if (list == NULL) return NULL;

	struct lnklist *tail = list, *ctr = list->next;
	for (; ctr != NULL; tail = ctr, ctr = ctr->next);

	return tail;
}

/*
 * push item onto list.
 */
_Bool
lnklist_push(struct lnklist *list, void *data)
{
	struct lnklist *new, *tail;
	if ((tail = lnklist_tail(list)) == NULL)
		return false;
	if ((new = lnklist_new()) == NULL)
		return false;

	tail->next = new;
	new->prev = tail;
	new->data = data;

	return true;
}

/* return a reference to the nth element. */
struct lnklist *
lnklist_ref(struct lnklist *list, size_t n)
{
	struct lnklist *head, *c;
	if ((head = lnklist_head(list)) == NULL)
		return NULL;
	for (c = head->next; c && n != 0; c = c->next)
		--n;
	return c;
}

/*
 * pop the last item off list and return its data.
 *
 * returns NULL if
 *     1) data is null
 */
void *
lnklist_pop(struct lnklist *list)
{
	struct lnklist *tail;
	if ((tail = lnklist_tail(list)) == NULL)
		return NULL;

	/* unlink */
	tail->prev->next = NULL;
	tail->prev = NULL;
	void *data = tail->data;
	free(tail);

	return data;
}

/*
 * get length of list, not counting head node.
 */
ssize_t
lnklist_len(struct lnklist *list)
{
	struct lnklist *head, *c;
	if ((head = lnklist_head(list)) == NULL)
		return -1;
	size_t len = 0;
	for (c = head->next; c; c = c->next) ++len;
	return len;
}

/*
 * free list. free'ing the list's data is your own
 * damn job.
 */
_Bool
lnklist_free(struct lnklist *list)
{
	/* rewind */
	struct lnklist *head, *c;
	if ((head = lnklist_head(list)) == NULL)
		return false;

	for (c = head->next; c != NULL; c = c->next)
		if (c->prev) free(c->prev);

	return true;
}
