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
 */
struct lnklist *
lnklist_head(struct lnklist *list)
{
	if (list == NULL) return NULL;

	struct lnklist *head = list, *c = list->prev;
	while (c) head = c, c = c->prev;

	return head;
}

/* get last node of list. */
struct lnklist *
lnklist_tail(struct lnklist *list)
{
	if (list == NULL) return NULL;

	struct lnklist *tail = list, *c = list->next;
	while (c) tail = c, c = c->next;

	return tail;
}

/* push item onto list. */
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

/* "Unhook" a node from the list and free it. */
_Bool
lnklist_rm(struct lnklist *node)
{
	/* decapitation bad */
	if (!node || !node->prev)
		return false;
	if (node->next)
		node->next->prev = node->prev;
	node->prev->next = node->next;
	node->next = node->prev = NULL;
	lnklist_free(node);
	return true;
}

/* Allocate and insert a new node after node `after' and set its
 * data to `data'. */
_Bool
lnklist_insert(struct lnklist *after, void *data)
{
	if (!after)
		return false;

	struct lnklist *new;
	if ((new = lnklist_new()) == NULL)
		return false;

	new->data = data;
	new->prev = after;
	if (after->next) {
		new->next = after->next;
		after->next->prev = new;
	}
	after->next = new;

	return true;
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
	struct lnklist *head, *ca, *cb;
	if ((head = lnklist_head(list)) == NULL)
		return false;
	for (ca = head, cb = head->next; cb; ca = cb, cb = cb->next)
		if (cb->prev) free(cb->prev);
	free(ca);

	return true;
}

/* free the list's data *and* the list using free(3). */
_Bool
lnklist_free_all(struct lnklist *list)
{
	struct lnklist *head, *c;
	if (!(head = lnklist_head(list))) return false;

	for (c = head; c; c = c->next)
		if (c->data) free(c->data);

	lnklist_free(list);
	return true;
}
