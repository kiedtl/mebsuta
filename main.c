/* See LICENSE file for license details. */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "conn.h"
#include "curl/url.h"
#include "gemini.h"
#include "termbox.h"
#include "ui.h"
#include "util.h"

/* maximum rate at which the screen is refreshed */
const struct timeval REFRESH = { 0, 1024 };

#define TIMEOUT 4096

/*
 * keep track of termbox's state, so
 * that we know if tb_shutdown() is safe to call,
 * and whether we can redraw the screen.
 *
 * calling tb_shutdown twice, or before tb_init,
 * results in a call to abort().
 */
size_t tb_status = 0;
const size_t TB_ACTIVE   = 0x01000000;
const size_t TB_MODIFIED = 0x02000000;

static const char *sigstrs[] = { [SIGILL]  = "SIGILL", [SIGSEGV] = "SIGSEGV",
	[SIGFPE]  = "SIGFPE", [SIGBUS]  = "SIGBUS", };

static void
signal_fatal(int sig)
{
	ui_shutdown();
	die("received signal %s (%d); aborting.",
		sigstrs[sig] ? sigstrs[sig] : "???", sig);
}

/*
 * check if (a) REFRESH time has passed, and (b) if the termbox
 * buffer has been modified; if both those conditions are met, "present"
 * the termbox screen.
 */
static inline void
tb_try_present(struct timeval *tcur, struct timeval *tpre)
{
	assert(gettimeofday(tcur, NULL) == 0);
	struct timeval diff;
	timersub(tcur, tpre, &diff);

	if (!timercmp(&diff, &REFRESH, >=))
		return;
	if ((tb_status & TB_MODIFIED) == TB_MODIFIED)
		tb_present();
}

int
main(void)
{
	/* register signal handlers */
	signal(SIGPIPE, SIG_IGN);

	struct sigaction fatal;
	fatal.sa_handler = &signal_fatal;
	sigaction(SIGILL,   &fatal, NULL);
	//sigaction(SIGSEGV,  &fatal, NULL);
	sigaction(SIGFPE,   &fatal, NULL);
	sigaction(SIGBUS,   &fatal, NULL);

	/*
	 * tpresent: last time tb_present() was called.
	 * tcurrent: buffer for gettimeofday(2).
	 */
	struct timeval tpresent = { 0, 0 };
	struct timeval tcurrent = { 0, 0 };

	assert(gettimeofday(&tpresent, NULL) == 0);

	/* incoming user events (key presses, window resizes,
	 * mouse clicks, etc */
	struct tb_event ev;
	int ret = 0;

	struct Gemdoc *g = NULL;
	CURLU *url = curl_url();
	curl_url_set(url, CURLUPART_URL,
		"gemini://gemini.circumlunar.space", 0);

	gemdoc_from_url(&g, url);

	ui_init();
	ui_doc = g;
	ui_display_gemdoc();

	_Bool quit = false;
	while ("the web sucks" && !quit) {
		tb_try_present(&tcurrent, &tpresent);

		if ((ret = tb_peek_event(&ev, 16)) == 0)
			continue;
		assert(ret != -1); /* termbox error */

		if (ev.type == TB_EVENT_KEY && ev.key) {
			switch (ev.key) {
			break; case TB_KEY_CTRL_C:
				quit = true;
			}
		} else if (ev.type == TB_EVENT_KEY && ev.ch) {
			if (isdigit(ev.ch)) {
				char *text = NULL;
				CURLU *url = NULL;

				if (gemdoc_find_link(g, ev.ch - '0', &text, &url)) {
					/* take a copy, since we'll be free'ing
					 * the gemdoc */
					url = curl_url_dup(url);

					gemdoc_free(g);
					g = NULL;

					/* DEBUG char *urll;
					curl_url_get(url, CURLUPART_URL, &urll, 0);
					ssize_t i = gemdoc_from_url(&g, url);
					die("url=%s,i=%d", urll, i); DEBUG */

					/* TODO: warn the user instead of dying */
					switch (gemdoc_from_url(&g, url)) {
					break; case -1:
						die("unknown scheme");
					break; case -2: case -3: case -4:
						die("error(tls): %s", tls_error(client));
					break; case -5:
						die("parse error");
					}

					ui_doc = g;
					ui_display_gemdoc();
					tb_present();
				}
			}

			switch(ev.ch) {
			break; case 'j':
				++ui_scroll;
				ui_display_gemdoc();
			break; case 'k':
				if (ui_scroll > 0) {
					--ui_scroll;
					ui_display_gemdoc();
				}
			}
		}
	}

	ui_shutdown();
	gemdoc_free(g);
	return 0;
}
