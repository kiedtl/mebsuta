#ifndef GEMINI_H
#define GEMINI_H

#include <stdint.h>
#include "curl/url.h"
#include "list.h"

#define GEM_DATA_HEADER1   1
#define GEM_DATA_HEADER2   2
#define GEM_DATA_HEADER3   3
#define GEM_DATA_TEXT      4
#define GEM_DATA_LIST      5
#define GEM_DATA_QUOTE     6
#define GEM_DATA_LINK      7
#define GEM_DATA_PREFORMAT 8

#define GEM_TYPE_INPUT     1
#define GEM_TYPE_SUCCESS   2
#define GEM_TYPE_REDIRECT  3
#define GEM_TYPE_TMPERROR  4
#define GEM_TYPE_ERROR     5
#define GEM_TYPE_NEEDCERT  6

#define GEM_STATUS_INPUT         10
#define GEM_STATUS_SENSINPUT     11
#define GEM_STATUS_SUCCESS       20
#define GEM_STATUS_TMPREDIRECT   30
#define GEM_STATUS_REDIRECT      31
#define GEM_STATUS_TMPERROR      40
#define GEM_STATUS_SRVOFFLINE    41
#define GEM_STATUS_CGIERROR      42
#define GEM_STATUS_PROXYERROR    43
#define GEM_STATUS_SLOWDOWN      44
#define GEM_STATUS_PERMERR       50
#define GEM_STATUS_NOTFOUND      51
#define GEM_STATUS_GONE          52
#define GEM_STATUS_NOTPROXY      53
#define GEM_STATUS_BADREQUEST    59
#define GEM_STATUS_CLCERTREQ     60
#define GEM_STATUS_CLCERTNOTAUTH 61
#define GEM_STATUS_CLCERTREQBAD  62

#define GEM_CHARSET_UNKNOWN 0
#define GEM_CHARSET_UTF8    1
#define GEM_CHARSET_UTF16   2
#define GEM_CHARSET_UTF7    3
#define GEM_CHARSET_ASCII   4

struct Gemtok {
	size_t type;
	char *text;
	char *raw_link_url;
	CURLU *link_url;
};

/* TODO: keep track of encoding, lang */
struct Gemdoc {
	CURLU *url;

	size_t status, type;
	char meta[(1024 - 3) + 1];

	struct lnklist *document;
	struct lnklist *rawdoc;

	/* parser state */
	_Bool __preformat_on;
	char  __preformat_alt[128];
	size_t __line, __links;
};

struct Gemdoc *gemdoc_new(CURLU *url);
ssize_t gemdoc_from_url(struct Gemdoc **g, CURLU *url);
_Bool gemdoc_parse(struct Gemdoc *g, char *line);
_Bool gemdoc_find_link(struct Gemdoc *g, size_t n, char **text, CURLU **url);
_Bool gemdoc_free(struct Gemdoc *g);

#endif
