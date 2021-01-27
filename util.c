#include <ctype.h>
#include <execinfo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

#include "ui.h"
#include "util.h"
#include "list.h"

void *
ecalloc(size_t nmemb, size_t size)
{
	void *m;
	if (!(m = calloc(nmemb, size)))
		die("Could not allocate %zu bytes:", nmemb * size);
	return m;
}

void
__ensure(_Bool expr, char *str, char *file, size_t line, const char *fn)
{
	if (expr) return;
	die("Assertion `%s' failed (%s:%s:%zu).", str, file, fn, line);
}

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

		if (!strings) {
			/* oopsie daisy, what went wrong here */
			fprintf(stderr, "Unable to provide backtrace.");
			_Exit(EXIT_FAILURE);
		}

		fprintf(stderr, "backtrace:\n");
		for (size_t i = 0; i < (size_t)nptrs; ++i)
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
	ENSURE((size_t) len < sizeof(buf));
	return (char *) &buf;
}

char *
strrep(char c, size_t n)
{
	static char buf[8192];
	ENSURE((n + 1) < (sizeof(buf) - 1));
	memset(buf, c, n);
	memset(buf, 0x0, n);
	return (char *) &buf;
}

size_t
stroverlap(const char *a, const char *b)
{
	size_t i = 0;
	while (a[i] && b[i] && a[i] == b[i])
		++i;
	return i;
}

char *
eat(char *s, int (*p)(int), size_t max)
{
	while (*s && p(*s) && max > 0)
		++s, --max;
	return s;
}

struct lnklist *
strfold(char *str, size_t width)
{
	struct lnklist *l = lnklist_new();
	ENSURE(l != NULL);

	char linebuf[8192], *p = linebuf, *spc = NULL;
	memset(linebuf, 0x0, sizeof(linebuf));

	if (width == strlen(str)) {
		lnklist_push(l, strdup(str));
		return l;
	}

	while (*str) {
		/* we're over width... */
		if ((size_t)(p - linebuf) >= width) {
			/* go back to the last space, and erase anything
			 * after it. reset our position to to that space. */
			if (spc) {
				size_t i = p - spc;
				str -= i, p = spc, spc = NULL;
				memset(p, 0x0, i);
			}

			/* add a new line */
			lnklist_push(l, strdup(linebuf));
			memset(linebuf, 0x0, sizeof(linebuf));
			p = linebuf;
		}

		/* we've found some whitespace. If we're at the
		 * beginning of the line, ignore it; otherwise, save it. */
		if (isblank(*str)) {
			if ((p - linebuf) == 0) {
				++str;
				continue;
			}

			spc = p;
		}

		*p = *str;
		++str, ++p;
	}

	/* push the last line */
	lnklist_push(l, strdup(linebuf));
	return l;
}

_Bool
utf8isblank(uint32_t ch)
{
	utf8proc_int32_t cch = (utf8proc_int32_t)ch;
	ENSURE(utf8proc_codepoint_valid(cch));

	switch (utf8proc_category(cch)) {
	case UTF8PROC_CATEGORY_ZS:
	case UTF8PROC_CATEGORY_ZL:
	case UTF8PROC_CATEGORY_ZP:
		return true;
	default:
		return false;
	}
}

/* XXX: chbuf should allocate 6 times as much memory, since a codepoint
 * can take up to 6 bytes when encoded */
void
utf8encode(uint32_t *utf8, size_t utf8sz, char *chbuf, size_t bufsz)
{
	unsigned char *p = (unsigned char *)chbuf;
	memset((void *)chbuf, 0x0, bufsz);
	for (size_t codepnt = 0; codepnt < utf8sz; ++codepnt) {
		p += utf8proc_encode_char(utf8[codepnt], p);
	}
}
