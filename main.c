/* See LICENSE file for license details. */

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "conn.h"
#include "config.h"
#include "curl/url.h"
#include "history.h"
#include "gemini.h"
#include "tabs.h"
#include "tbrl.h"
#include "termbox.h"
#include "ui.h"
#include "util.h"

static void
handlesig(int sig)
{
	static const char *sigstrs[] = {
		[SIGILL]  = "SIGILL", [SIGSEGV] = "SIGSEGV",
		[SIGFPE]  = "SIGFPE", [SIGBUS]  = "SIGBUS"
	};

	die("received signal %s (%d); aborting.",
		sigstrs[sig] ? sigstrs[sig] : "???", sig);
}

static ssize_t
make_request(struct Gemdoc **g, CURLU *url, char **e)
{
	ENSURE(g), ENSURE(url);

	ssize_t status = 0;
	*g = gemdoc_new(url);

	char bufsrv[65536]; /* buffer for server data */
	memset(bufsrv, 0x0, sizeof(bufsrv));
	size_t rc = 0;      /* received */
	ssize_t r = 0;      /* return code of conn_recv */
	size_t max = sizeof(bufsrv) - 1;

	CURLUcode err;
	char *scheme, *host, *port, *clurl;

	/* wait, did you say gopher? */
	err = curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
	if (err || strcmp(scheme, "gemini")) return -1;

	conn_init();
	err = curl_url_get(url, CURLUPART_HOST, &host, 0);
	err = curl_url_get(url, CURLUPART_PORT, &port, 0);
	err = curl_url_get(url, CURLUPART_URL, &clurl, 0);

	if (!conn_conn(host, port ? port : "1965")) {
		status = -2;
		goto cleanup;
	}

	if (!conn_send(format("%s\r\n", clurl))) {
		status = -3;
		goto cleanup;
	}

	gemdoc_ctx_t *ctx = gemdoc_parse_init();

	while ((r = conn_recv(bufsrv, max - 1 - rc)) != -1) {
		if (r == -2) {
			status = -4;
			goto cleanup;
		} else if (r == 0)
			continue;

		rc += r;
		char *end, *ptr = (char *) &bufsrv;

		while ((end = memchr(ptr, '\n', &bufsrv[rc] - ptr))) {
			*end = '\0';
			if (!gemdoc_parse(ctx, *g, ptr)) {
				status = -5;
				goto cleanup;
			}
			ptr = end + 1;
		}

		rc -= ptr - bufsrv;
		memmove(&bufsrv, ptr, rc);
	};

	gemdoc_parse_finish(ctx, *g);

cleanup:
	free(scheme);
	free(host);
	free(port);
	free(clurl);

	if (status != 0 && e != NULL) {
		*e = (char *)tls_error(client);
		if (*e) *e = strdup(*e);
		else *e = strdup(strerror(errno));
	}

	if (conn_active()) conn_close();
	conn_shutdown();

	return status;
}

static void
follow_link(CURLU *url, size_t redirects)
{
	/* take a copy, as url may be a reference to another gemdoc's
	 * url, which we'll free separately */
	url = curl_url_dup(url);

	char *error;

	struct Gemdoc *newdoc;
	switch (make_request(&newdoc, url, &error)) {
	break; case -1:;
		char *scheme;
		curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
		ui_message(UI_STOP, "Unsupported URL scheme '%s'", scheme);
		free(scheme);
	break; case -2: case -3: case -4:;
		ui_message(UI_STOP, "error: %s", error);
		if (error) free(error);
	break; case -5:
		ui_message(UI_STOP, "Could not parse document.");
	}

	/* now let's check for input and redirects */
	switch (newdoc->type) {
	break; case GEM_TYPE_INPUT:
		tbrl_setbuf(":input ");
	break; case GEM_TYPE_REDIRECT:;
		CURLUcode error;
		CURLU *rurl = curl_url_dup(newdoc->url);
		error = curl_url_set(rurl, CURLUPART_URL, newdoc->meta, 0);
		if (error) {
			ui_message(UI_WARN, "Invalid redirect URL.");
			goto show;
		}

		if (c_automatic_redirects
				&& redirects < c_maximum_redirects) {
			follow_link(rurl, redirects + 1);
		} else {
			char *urlbuf;
			curl_url_get(rurl, CURLUPART_URL, &urlbuf, 0);
			tbrl_setbuf(format(":go %s", urlbuf));
			free(urlbuf);
		}
	}

show:
	hist_add(&CURTAB()->visited, newdoc);
	ui_redraw();
}

static void
newtab(CURLU *url)
{
	tabs_add(curtab), curtab = curtab->next;
	follow_link(url, 0);
}

static void
editurl(void)
{
	char *urlbuf;
	curl_url_get(CURDOC()->url, CURLUPART_URL, &urlbuf, 0);
	tbrl_setbuf(format(":go %s", urlbuf));
	free(urlbuf);
	ui_redraw();
}

#include "commands.c"

int
main(void)
{
	/* register signal handlers */
	signal(SIGPIPE, SIG_IGN);
	struct sigaction hnd = { .sa_handler = &handlesig };
	size_t sigs[] = { SIGILL, SIGSEGV, SIGFPE, SIGBUS };
	for (size_t i = 0; i < SIZEOF(sigs); ++i)
		sigaction(sigs[i], &hnd, NULL);

	CURLU *homepage_curl = curl_url();
	curl_url_set(homepage_curl, CURLUPART_URL, homepage, 0);

	ui_init();
	tabs_init();

	newtab(curl_url_dup(homepage_curl));
	ui_redraw();

	tbrl_init();
	tbrl_complete_callback = &command_complete;
	tbrl_enter_callback = &command_run;

	/* incoming user events (key presses, window resizes,
	 * mouse clicks, etc */
	struct tb_event ev;
	int ret = 0;

	_Bool quit = false;
	while ("the web sucks" && !quit) {
		ui_present();

		if ((ret = tb_peek_event(&ev, 16)) == 0)
			continue;
		ENSURE(ret != -1); /* termbox error */

		if (tbrl_len() > 0) {
			tbrl_handle(&ev);
			ui_redraw();
			continue;
		}

		if (ev.type == TB_EVENT_KEY && ev.key) {
			switch (ev.key) {
			break; case TB_KEY_CTRL_C:
				quit = true;
			break; case TB_KEY_CTRL_L:
				/* redraw */
			break; case TB_KEY_SPACE:
				CURTAB()->ui_vscroll += tb_height();
			break; case TB_KEY_CTRL_T:
				newtab(curl_url_dup(homepage_curl));
			break; case TB_KEY_CTRL_W:
				if (!curtab || tabs_len() == 1)
					break;
				if (curtab->next && curtab->next->data) {
					curtab = curtab->next;
					tabs_rm(curtab->prev);
				} else if (curtab->prev && curtab->prev->data) {
					curtab = curtab->prev;
					tabs_rm(curtab->next);
				}
			break; case TB_KEY_CTRL_P:
				if (curtab->prev && curtab->prev->data)
					curtab = curtab->prev;
			break; case TB_KEY_CTRL_N:
				if (curtab->next && curtab->next->data)
					curtab = curtab->next;
			break; default:
				ui_handle(&ev);
			}

			ui_redraw();
		} else if (ev.type == TB_EVENT_KEY && ev.ch) {
			switch(ev.ch) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':;
				CURLU *u = NULL;
				size_t l = ev.ch - '0';

				if (!gemdoc_find_link(CURDOC(), l, NULL, &u))
					break;

				follow_link(u, 0);
				curl_url_cleanup(u);
			break; case 'g':
				CURTAB()->ui_vscroll = 0;
			break; case 'G':
				CURTAB()->ui_vscroll = ui_redraw() - 10;
			break; case 'j':
				++CURTAB()->ui_vscroll;
			break; case 'k':
				if (CURTAB()->ui_vscroll > 0)
					--CURTAB()->ui_vscroll;
			break; case 'h':
				if (CURTAB()->ui_hscroll > 0)
					--CURTAB()->ui_hscroll;
			break; case 'l':
				++CURTAB()->ui_hscroll;
			break; case 'b':
				hist_back(&CURTAB()->visited);
			break; case 'f':
				hist_forw(&CURTAB()->visited);
			break; case 'r':
				follow_link(CURDOC()->url, 0);
			break; case ':':
				tbrl_handle(&ev);
			break; case ';':
				tbrl_setbuf(":go ");
			break; case '[':
				tbrl_setbuf(":newgo ");
			break; case 'e':
				editurl();
			break; default:
				ui_handle(&ev);
			}

			ui_redraw();
		}
	}

	ui_shutdown();
	tabs_free();
	curl_url_cleanup(homepage_curl);

	return 0;
}
