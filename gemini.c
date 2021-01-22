#define _GNU_SOURCE

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "curl/url.h"

#include "conn.h"
#include "gemini.h"
#include "list.h"
#include "strlcpy.h"
#include "util.h"

/* parser state */
struct Gemdoc_CTX {
	_Bool preformat_on;
	char  preformat_alt[128];
	size_t line, links;
};

static _Bool
_parse_responseline(struct Gemdoc *g, char *line)
{
	if (!isdigit(line[0]))
		return false;

	g->type = line[0] - '0';
	g->status = (g->type*10) + (line[1]-'0');

	strlcpy(g->meta, &line[3], sizeof(g->meta));

	return true;
}

static size_t
_line_type(struct Gemdoc_CTX *ctx, char **line)
{
	static size_t markersz[] = {
		[GEM_DATA_HEADER3]   = 3,
		[GEM_DATA_HEADER2]   = 2,
		[GEM_DATA_HEADER1]   = 1,
		[GEM_DATA_QUOTE]     = 1,
		[GEM_DATA_LINK]      = 2,
		[GEM_DATA_LIST]      = 2,
		[GEM_DATA_TEXT]      = 0,
		[GEM_DATA_PREFORMAT] = 0,
	};

	if (ctx->preformat_on)
		return GEM_DATA_PREFORMAT;

	size_t type;

	if (!strncmp(*line, "###", 3))
		type = GEM_DATA_HEADER3;
	else if (!strncmp(*line, "##", 2))
		type = GEM_DATA_HEADER2;
	else if (!strncmp(*line, "#", 1))
		type = GEM_DATA_HEADER1;
	else if (!strncmp(*line, ">", 1))
		type = GEM_DATA_QUOTE;
	else if (!strncmp(*line, "=>", 2))
		type = GEM_DATA_LINK;
	else if (!strncmp(*line, "* ", 2))
		type = GEM_DATA_LIST;
	else
		type = GEM_DATA_TEXT;

	*line += markersz[type];
	return type;
}

/* Try to extract the title from a "20 text/gemini" reply by grabbing the first
 * title/text from the page. Not perfect, but it works most of the time. */
static size_t
_extract_title(struct Gemdoc *g, char *buf, size_t bufsz)
{
	ENSURE(g), ENSURE(g->type == GEM_TYPE_SUCCESS);

	struct lnklist *t;
	struct Gemtok *tok;
	size_t tries[] = { GEM_DATA_HEADER1, GEM_DATA_TEXT };

	for (size_t try = 0; try < SIZEOF(tries); ++try) {
		for (t = g->document->next; t; t = t->next) {
			tok = (struct Gemtok *)t->data;
			if (tok->type == try)
				return strlcpy(buf, tok->text, bufsz);
		}
	}

	char *tmp;
	ENSURE(!curl_url_get(g->url, CURLUPART_HOST, &tmp, 0));
	size_t s = strlcpy(g->title, tmp, sizeof(g->title));
	free(tmp);

	return s;
}

static void
_set_title(struct Gemdoc *g)
{
	size_t sz = sizeof(g->title);

	memset(g->title, 0x0, sz);

	if (_extract_title(g, g->title, sz) >= sz)
		g->title[sz-2] = g->title[sz-3] = g->title[sz-4] = '.';
}

struct Gemdoc *
gemdoc_new(CURLU *url)
{
	struct Gemdoc *g = calloc(1, sizeof(struct Gemdoc));
	ENSURE(g);
	g->url = url;
	g->document = lnklist_new();
	g->rawdoc = lnklist_new();

	return g;
}

struct Gemdoc_CTX *
gemdoc_parse_init(void)
{
	struct Gemdoc_CTX *c = calloc(1, sizeof(struct Gemdoc_CTX));
	ENSURE(c);

	c->preformat_on = false;
	memset(c->preformat_alt, 0x0, sizeof(c->preformat_alt));
	c->line = c->links = 0;

	return c;
}

_Bool
gemdoc_parse(struct Gemdoc_CTX *ctx, struct Gemdoc *g, char *line)
{
	/* store a pointer to the beginning of the line, just in case we
	 * need to back up and restore the line after moving the ptr forward */
	char *begin = line;

	/* remove trailing \r, if any */
	char *cr;
	if ((cr = strchr(line, '\r')))
		*cr = '\0';

	++ctx->line;
	lnklist_push(g->rawdoc, strdup(line));

	if (ctx->line == 1) {
		/* We're on the first line. Parse the status code and
		 * the meta text and bail out. */
		return _parse_responseline(g, line);
	} else if (ctx->line == 2 && strlen(line) == 0) {
		/* ignore blank line after response code */
		return true;
	}

	if (!strncmp(line, "```", 3)) {
		ctx->preformat_on = !ctx->preformat_on;
		memset(ctx->preformat_alt, 0x0, sizeof(ctx->preformat_alt));
		if (ctx->preformat_on)
			strlcpy(ctx->preformat_alt, &line[3], sizeof(ctx->preformat_alt));
		return true;
	}

	size_t type = _line_type(ctx, &line);
	struct Gemtok *gdl = calloc(1, sizeof(struct Gemtok));
	ENSURE(gdl);

	if (type == GEM_DATA_LINK) {
		gdl->type = type;

		char *end;

		/* 1) Remove spaces after the "=>" */
		while (*line && isblank(*line)) ++line;

		/* 2) Find the end of the URL, and copy it */
		for (end = line; *end && !isblank(*end); ++end);
		gdl->raw_link_url = strndup(line, end - line);

		if (*end) {
			/* 3) Remove spaces after the end of the URL */
			for (line = ++end; *line && isblank(*line); ++line);

			/* 4) Grab the URL's alt text */
			gdl->text = strdup(end);
		}

		CURLUcode c_rc;

		/* Start with the document's URL before adding the link's URL.
		 * This allows relative URLs to be handled properly by cURL. */
		gdl->link_url = curl_url_dup(g->url);
		c_rc = curl_url_set(gdl->link_url, CURLUPART_URL, gdl->raw_link_url, 0);

		switch (c_rc) {
		case CURLUE_OK:
			break; /* yey */
		case CURLUE_MALFORMED_INPUT:
		default:
			free(gdl->text);
			free(gdl->raw_link_url);
			curl_url_cleanup(gdl->link_url);

			gdl->type = GEM_DATA_TEXT;
			gdl->text = strdup(begin);
			gdl->link_url = NULL, gdl->raw_link_url = NULL;
			lnklist_push(g->document, (void *) gdl);
			return true;
		}

		lnklist_push(g->document, (void *) gdl);
	} else {
		if (type != GEM_DATA_PREFORMAT)
			while (*line && isblank(*line)) ++line;
		gdl->type = type;
		gdl->text = strdup(line);
		lnklist_push(g->document, (void *)gdl);
	}

	return true;
}

_Bool
gemdoc_parse_finish(struct Gemdoc_CTX *ctx, struct Gemdoc *g)
{
	ENSURE(ctx), ENSURE(g);

	free(ctx);
	_set_title(g);

	return true;
}

_Bool
gemdoc_find_link(struct Gemdoc *g, size_t n, char **text, CURLU **url)
{
	struct lnklist *c;
	for (c = g->document->next; c; c = c->next) {
		struct Gemtok *l = ((struct Gemtok *)c->data);
		if (l->type == GEM_DATA_LINK && --n == 0) {
			*text = l->text, *url = l->link_url;
			return true;
		}
	}
	return false;
}

_Bool
gemdoc_free(struct Gemdoc *g)
{
	if (!g) return false;

	struct lnklist *c;

	for (c = g->document->next; c; c = c->next) {
		if (!c->data) continue;
		struct Gemtok *l = ((struct Gemtok *)c->data);

		if (l->type == GEM_DATA_LINK) {
			curl_url_cleanup(l->link_url);
			if (l->raw_link_url) free(l->raw_link_url);
		}

		if (l->text) free(l->text);
	}

	if (g->rawdoc)   lnklist_free_all(g->rawdoc);
	if (g->document) lnklist_free_all(g->document);
	if (g->url)      curl_url_cleanup(g->url);

	free(g);
	return true;
}
