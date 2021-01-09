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

static size_t
_line_type(struct Gemdoc *g, char **line)
{
	if (g->__preformat_on)
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

struct Gemdoc *
gemdoc_new(CURLU *url)
{
	struct Gemdoc *g = calloc(1, sizeof(struct Gemdoc));

	g->url = url;
	g->document = lnklist_new(), g->rawdoc = lnklist_new();
	g->__line = g->__links = g->__preformat_on = 0;

	return g;
}

ssize_t
gemdoc_from_url(struct Gemdoc **g, CURLU *url)
{
	*g = gemdoc_new(url);

	char bufsrv[4096]; /* buffer for server data */
	size_t rc = 0; /* received */
	ssize_t r = 0; /* return code of conn_recv */
	size_t max = sizeof(bufsrv) - 1;

	conn_init();

	CURLUcode c_rc;

	/* wait, did you say gopher? */
	char *scheme, *host, *clurl;
	c_rc = curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
	if (c_rc || strcmp(scheme, "gemini")) return c_rc;
	free(scheme);

	c_rc = curl_url_get(url, CURLUPART_HOST, &host, 0);
	c_rc = curl_url_get(url, CURLUPART_URL, &clurl, 0);

	if (!conn_conn(host, "1965")) return -2;
	free(host);

	if (!conn_send(clurl) || !conn_send("\r\n")) return -3;
	free(clurl);

	while ((r = conn_recv(bufsrv, max)) != -1) {
		if (r == -2) return -4;
		else if (r ==  0) continue;

		rc += r;
		char *end, *ptr = (char *) &bufsrv;

		while ((end = memchr(ptr, '\n', &bufsrv[rc] - ptr))) {
			*end = '\0';
			if (!gemdoc_parse(*g, ptr))
				return -5;
			ptr = end + 1;
		}

		rc -= ptr - bufsrv;
		memmove(&bufsrv, ptr, rc);
	};

	conn_close();
	conn_shutdown();
	return 0;
}

_Bool
gemdoc_parse(struct Gemdoc *g, char *line)
{
	/* store a pointer to the beginning of the line...
	 * just in case */
	char *begin = line;

	/* remove trailing \r, if any */
	char *cr;
	if ((cr = strchr(line, '\r')))
		*cr = '\0';

	++g->__line;
	lnklist_push(g->rawdoc, strdup(line));

	if (g->__line == 1) {
		if (line[0] > '9' || line[0] < '0')
			return false;
		g->type = line[0] - '0';
		g->status = (g->type * 10) + (line[1] - '0');
		strlcpy(g->meta, &line[3], sizeof(g->meta) - 1);
		return true;
	} else if (g->__line == 2 && strlen(line) == 0) {
		/* ignore blank line after response code */
		return true;
	}

	if (!strncmp(line, "```", 3)) {
		g->__preformat_on = !g->__preformat_on;
		if (g->__preformat_on) {
			strlcpy(g->__preformat_alt, &line[3],
				sizeof(g->__preformat_alt) - 1);
		} else {
			g->__preformat_alt[0] = '\0';
		}
		return true;
	}

	size_t type = _line_type(g, &line);
	struct Gemtok *gdl = calloc(1, sizeof(struct Gemtok));

	if (type == GEM_DATA_LINK) {
		char *end;

		/* 1) Remove spaces after the "=>"
		 * 2) Find the end of the URL, and copy it
		 * 3) Remove spaces after the end of the URL
		 * 4) Grab the URL's alt text */
		while (*line && isblank(*line)) ++line;
		for (end = line; *end && !isblank(*end); ++end);
		gdl->raw_link_url = strndup(line, end - line);

		if (*end) {
			for (line = ++end; *line && isblank(*line); ++line);
			gdl->text = strdup(end);
		}

		gdl->type = type;

		/* add url of document, then add url of link.
		 * this allows relative urls. */
		gdl->link_url = curl_url_dup(g->url);
		CURLUcode c_rc = curl_url_set(gdl->link_url,
				CURLUPART_URL, gdl->raw_link_url, 0);

		switch (c_rc) {
		break; case CURLUE_OK:
			break; /* yey */
		break; case CURLUE_MALFORMED_INPUT:
			/* just.. do nothing.
			 * FIXME: maybe we should print a warning
			 * or something? */
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
		while (*line && isblank(*line)) ++line;
		gdl->type = type;
		gdl->text = strdup(line);
		lnklist_push(g->document, (void *) gdl);
	}

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
	curl_url_cleanup(g->url);

	struct lnklist *c;

	for (c = g->rawdoc->next; c; c = c->next)
		if (c->data) free(c->data);

	for (c = g->document->next; c; c = c->next) {
		if (!c->data) continue;
		struct Gemtok *l = ((struct Gemtok *)c->data);

		if (l->type == GEM_DATA_LINK)
			curl_url_cleanup(l->link_url);

		if (l->text)
			free(l->text);

		free(l);
	}

	lnklist_free(g->rawdoc);
	lnklist_free(g->document);
	free(g);

	return true;
}
