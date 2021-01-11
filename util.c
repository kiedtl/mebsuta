#include <assert.h>
#include <ctype.h>
#include <execinfo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "util.h"
#include "list.h"

_Noreturn void __attribute__((format(printf, 1, 2)))
die(const char *fmt, ...)
{
	ui_shutdown();

	fprintf(stderr, "fatal: ");

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		perror(" ");
	} else {
		fputc('\n', stderr);
	}

	char *buf_sz_str = getenv("MEBS_DEBUG");

	if (buf_sz_str == NULL) {
		fprintf(stderr, "NOTE: set $MEBS_DEBUG >0 for a backtrace.\n");
	} else {
		size_t buf_sz = strtol(buf_sz_str, NULL, 10);
		void *buffer[buf_sz];

		int nptrs = backtrace(buffer, buf_sz);
		char **strings = backtrace_symbols(buffer, nptrs);
		assert(strings);

		fprintf(stderr, "backtrace:\n");
		for (size_t i = 0; i < (size_t) nptrs; ++i)
			fprintf(stderr, "   %s\n", strings[i]);
		free(strings);
	}

	_Exit(EXIT_FAILURE);
}

char * __attribute__((format(printf, 1, 2)))
format(const char *fmt, ...)
{
	static char buf[8192];
	memset(buf, 0x0, sizeof(buf));
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	assert((size_t) len < sizeof(buf));
	return (char *) &buf;
}

char *
strrep(char c, size_t n)
{
	static char buf[8192];
	assert((n + 1) < (sizeof(buf) - 1));
	memset(buf, c, n);
	buf[n + 1] = '\0';
	return (char *) &buf;
}

struct lnklist *
strfold(char *str, size_t width)
{
	struct lnklist *l = lnklist_new();
	assert(l != NULL);

	char buf[8192], *p = buf, *spc = NULL;
	memset(buf, 0x0, sizeof(buf));

	while (*str) {
		/* we're over width... */
		if ((size_t)(p - buf) >= width) {
			/* go back to the last space, and erase
			 * anything after it. reset our position to
			 * that space. */
			if (spc) {
				size_t i = p - spc;
				str -= i, p = spc, spc = NULL;
				memset(p, 0x0, i);
			}

			/* add a new line */
			lnklist_push(l, strdup(buf));
			memset(buf, 0x0, sizeof(buf));
			p = buf;
		}

		/* we've found some whitespace.
		 * if we're at the beginning of the line, we should
		 * ignore it; otherwise, save it. */
		if (isblank(*str)) {
			if ((p - buf) == 0) {
				++str;
				continue;
			}

			spc = p;
		}

		*p = *str;
		++str, ++p;
	}

	/* push the last line */
	lnklist_push(l, strdup(buf));
	return l;
}
