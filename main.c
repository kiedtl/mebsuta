/* See LICENSE file for license details. */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#include "conn.h"
#include "curl/url.h"
#include "gemini.h"
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
		ui_present();

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
				}
			}

			switch(ev.ch) {
			break; case 'j':
				++ui_vscroll;
				ui_display_gemdoc();
			break; case 'k':
				if (ui_vscroll > 0) {
					--ui_vscroll;
					ui_display_gemdoc();
				}
			break; case 'h':
				if (ui_hscroll > 0) {
					--ui_hscroll;
					ui_display_gemdoc();
				}
			break; case 'l':
				++ui_hscroll;
				ui_display_gemdoc();
			}
		}
	}

	ui_shutdown();
	gemdoc_free(g);
	return 0;
}
