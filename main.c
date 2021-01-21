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
signal_fatal(int sig)
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

	char *error;

	switch (make_request(&CURTAB().doc, url, &error)) {
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
	switch (CURTAB().doc->type) {
	break; case GEM_TYPE_INPUT:
		tbrl_setbuf(":input ");
	break; case GEM_TYPE_REDIRECT:;
		CURLUcode error;
		CURLU *rurl = curl_url_dup(CURTAB().doc->url);
		error = curl_url_set(rurl, CURLUPART_URL, CURTAB().doc->meta, 0);
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
	hist_add(&CURTAB().hist, CURTAB().doc);
	ui_redraw();
}

static void
newtab(CURLU *url)
{
	tab_cur = tabs_len();
	make_request(&CURTAB().doc, url, NULL);
	hist_add(&CURTAB().hist, CURTAB().doc);
}

static void
editurl(void)
{
	char *urlbuf;
	curl_url_get(CURTAB().doc->url, CURLUPART_URL, &urlbuf, 0);
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
	struct sigaction fatal;
	fatal.sa_handler = &signal_fatal;
	sigaction(SIGILL,   &fatal, NULL);
	sigaction(SIGSEGV,  &fatal, NULL);
	sigaction(SIGFPE,   &fatal, NULL);
	sigaction(SIGBUS,   &fatal, NULL);

	CURLU *homepage_curl = curl_url();
	curl_url_set(homepage_curl, CURLUPART_URL, homepage, 0);
	struct Gemdoc *tmp;

	tabs_init();
	newtab(homepage_curl);

	tbrl_init();
	tbrl_complete_callback = &command_complete;
	tbrl_enter_callback = &command_run;

	ui_init();
	ui_redraw();

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
				editurl();
			break; case TB_KEY_SPACE:
				CURTAB().ui_vscroll += tb_height();
			break; case TB_KEY_CTRL_T:
				newtab(homepage_curl);
			break; case TB_KEY_CTRL_W:
				/* TODO */
			break; case TB_KEY_CTRL_P:
				if (tab_cur > 0)
					--tab_cur;
			break; case TB_KEY_CTRL_N:
				if (tab_cur < tabs_len())
					++tab_cur;
			break; default:
				ui_handle(&ev);
			}

			ui_redraw();
		} else if (ev.type == TB_EVENT_KEY && ev.ch) {
			switch(ev.ch) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':;
				char *text = NULL;
				CURLU *url = NULL;

				if (!gemdoc_find_link(CURTAB().doc, ev.ch - '0', &text, &url))
					break;

				follow_link(url, 0);
			break; case 'g':
				CURTAB().ui_vscroll = 0;
				ui_redraw();
			break; case 'G':
				CURTAB().ui_vscroll = ui_redraw() - 10;
				ui_redraw();
			break; case 'j':
				++CURTAB().ui_vscroll;
				ui_redraw();
			break; case 'k':
				if (CURTAB().ui_vscroll > 0) {
					--CURTAB().ui_vscroll;
					ui_redraw();
				}
			break; case 'h':
				if (CURTAB().ui_hscroll > 0) {
					--CURTAB().ui_hscroll;
					ui_redraw();
				}
			break; case 'l':
				++CURTAB().ui_hscroll;
				ui_redraw();
			break; case 'b':
				if (hist_len(&CURTAB().hist) == 0)
					break;
				tmp = CURTAB().doc;
				if (!(CURTAB().doc = hist_back(&CURTAB().hist))) {
					CURTAB().doc = tmp;
					break;
				}
				ui_redraw();
			break; case 'f':
				if (hist_len(&CURTAB().hist) == 0)
					break;
				tmp = CURTAB().doc;
				if (!(CURTAB().doc = hist_forw(&CURTAB().hist))) {
					CURTAB().doc = tmp;
					break;
				}
				ui_redraw();
			break; case 'r':
				follow_link(CURTAB().doc->url, 0);
			break; case ':':
				tbrl_handle(&ev);
				ui_redraw();
			break; case ';':
				tbrl_setbuf(":go ");
				ui_redraw();
			break; case '[':
				tbrl_setbuf(":newgo ");
				ui_redraw();
			break; case 'e':
				editurl();
			break; default:
				ui_handle(&ev);
				ui_redraw();
			}
		}
	}

	tabs_free();
	ui_shutdown();
	return 0;
}
