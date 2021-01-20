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
#include "tbrl.h"
#include "termbox.h"
#include "ui.h"
#include "util.h"

static void
signal_fatal(int sig)
{
	static const char *sigstrs[] = {
		[SIGILL]  = "SIGILL", [SIGSEGV] = "SIGSEGV",
		[SIGFPE]  = "SIGFPE", [SIGBUS]  = "SIGBUS"
	};

	die("received signal %s (%d); aborting.",
		sigstrs[sig] ? sigstrs[sig] : "???", sig);
}

static struct Gemdoc *g = NULL, *old = NULL;

static ssize_t
make_request(struct Gemdoc **g, CURLU *url, char **e)
{
	conn_init();
	ssize_t r = gemdoc_from_url(g, url);
	if (r != 0 && e != NULL) {
		*e = (char *)tls_error(client);
		if (*e) *e = strdup(*e);
		else *e = strdup(strerror(errno));
	}
	if (conn_active()) conn_close();
	conn_shutdown();
	return r;
}

static void
follow_link(CURLU *url, size_t redirects)
{
	/* take a copy, as url may be a reference to another gemdoc's
	 * url, which we'll free separately */
	url = curl_url_dup(url);

	g = NULL;
	char *error;

	switch (make_request(&g, url, &error)) {
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
	switch (g->type) {
	break; case GEM_TYPE_INPUT:
		tbrl_setbuf(":input ");
	break; case GEM_TYPE_REDIRECT:;
		CURLUcode error;
		CURLU *rurl = curl_url_dup(g->url);
		error = curl_url_set(rurl, CURLUPART_URL, g->meta, 0);
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
	hist_add(g);
	ui_set_gemdoc(g);
	ui_redraw();
}

#include "commands.c"

int
main(void)
{
	/* --- DEBUG TRASH, ignore ---
	CURLU *u, *au = curl_url(), *bu = curl_url(), *cu = curl_url(), *du = curl_url();
	struct Gemdoc *a, *b, *c, *d;

	curl_url_set(au, CURLUPART_URL, "gemini://gemini.circumlunar.space", 0);
	gemdoc_from_url(&a, au);
	curl_url_set(bu, CURLUPART_URL, "gemini://tilde.team/", 0);
	gemdoc_from_url(&b, bu);
	curl_url_set(cu, CURLUPART_URL, "gemini://cosmic.voyage/", 0);
	gemdoc_from_url(&c, cu);
	curl_url_set(du, CURLUPART_URL,"gemini://gemini.ctrl-c.club", 0);
	gemdoc_from_url(&d, du);

	hist_init();
	hist_add(a);
	hist_add(b);
	hist_add(c);
	hist_back();
	hist_back();
	hist_add(d);
	hist_add(a);
	char *urll;
	for (size_t i = 0; i < hist_len(); ++i) {
		if (!history[i]) continue;
		u = history[i]->url;
		curl_url_get(u, CURLUPART_URL, &urll, 0);
		printf("[%zu] '%s'\n", i, urll);
	}
	printf("done (histlen=%zu, histpos=%zu)\n", hist_len(), histpos);
	return 0;*/

	/* register signal handlers */
	signal(SIGPIPE, SIG_IGN);

	struct sigaction fatal;
	fatal.sa_handler = &signal_fatal;
	sigaction(SIGILL,   &fatal, NULL);
	sigaction(SIGSEGV,  &fatal, NULL);
	sigaction(SIGFPE,   &fatal, NULL);
	sigaction(SIGBUS,   &fatal, NULL);

	CURLU *url = curl_url();
	curl_url_set(url, CURLUPART_URL, homepage, 0);

	make_request(&g, url, NULL);

	hist_init();
	hist_add(g);

	tbrl_init();
	tbrl_complete_callback = &command_complete;
	tbrl_enter_callback = &command_run;

	ui_init();
	ui_set_gemdoc(g);

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
				ui_set_gemdoc(g);
			break; case TB_KEY_SPACE:
				ui_vscroll += tb_height();
				ui_redraw();
			break; default:
				ui_handle(&ev);
				ui_redraw();
			}
		} else if (ev.type == TB_EVENT_KEY && ev.ch) {
			switch(ev.ch) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':;
				char *text = NULL;
				CURLU *url = NULL;

				if (!gemdoc_find_link(g, ev.ch - '0', &text, &url))
					break;

				follow_link(url, 0);
			break; case 'g':
				ui_vscroll = 0;
				ui_redraw();
			break; case 'G':
				ui_vscroll = ui_redraw() - 10;
				ui_redraw();
			break; case 'j':
				++ui_vscroll;
				ui_redraw();
			break; case 'k':
				if (ui_vscroll > 0) {
					--ui_vscroll;
					ui_redraw();
				}
			break; case 'h':
				if (ui_hscroll > 0) {
					--ui_hscroll;
					ui_redraw();
				}
			break; case 'l':
				++ui_hscroll;
				ui_redraw();
			break; case 'b':
				if (hist_len() == 0)
					break;

				old = g;
				if (!(g = hist_back())) {
					g = old;
					break;
				}

				ui_set_gemdoc(g);
			break; case 'f':
				if (hist_len() == 0)
					break;

				old = g;
				if (!(g = hist_forw())) {
					g = old;
					break;
				}

				ui_set_gemdoc(g);
			break; case 'r':
				follow_link(g->url, 0);
			break; case ':':
				tbrl_handle(&ev);
				ui_redraw();
			break; case ';':
				tbrl_setbuf(":go ");
				ui_redraw();
			break; case 'e':;
				char *urlbuf;
				curl_url_get(g->url, CURLUPART_URL, &urlbuf, 0);
				tbrl_setbuf(format(":go %s", urlbuf));
				free(urlbuf);
				ui_redraw();
			break; default:
				ui_handle(&ev);
				ui_redraw();
			}
		}
	}

	ui_shutdown();
	hist_free();
	return 0;
}
