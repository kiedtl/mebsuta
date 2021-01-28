#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "util.h"
#include "curl/url.h"
#include "gemini.h"

static _Bool c_automatic_redirects = true;
static size_t  c_maximum_redirects = 5;

static char *homepage = "gemini://gemini.circumlunar.space";

static char * __attribute__((unused))
statusline(size_t width, size_t read, struct Gemdoc *g)
{
	char lstatus[100] = { '\0' }, rstatus[100] = { '\0' };
	strcpy(lstatus, format("%3d%%", read));
	strcat(lstatus, format(" %s/%s", g->mime, g->submime));

	char *url;
	curl_url_get(g->url, CURLUPART_URL, &url, 0);
	strcpy(rstatus, format("%s", url));
	free(url);

	char *pad = strrep(' ', width - strlen(lstatus) - strlen(rstatus) - 2);

	return format("\0030,252 %s%s%s \x0f", lstatus, pad, rstatus);
}

#endif
