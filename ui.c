#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <utf8proc.h>

#include "curl/url.h"
#include "gemini.h"
#include "history.h"
#include "list.h"
#include "mirc.h"
#include "tbrl.h"
#include "termbox.h"
#include "ui.h"
#include "util.h"

/* maximum rate at which the screen is refreshed */
static const struct timeval REFRESH = { 0, 1024 };

static const char *DISMISS = "-- Press <Enter> to dismiss --";
char ui_messagebuf[255];
enum UiMessageType ui_message_type;

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

size_t ui_hscroll = 0, ui_vscroll = 0;
struct Gemdoc *ui_doc = NULL;
_Bool ui_raw_doc = false;

/*
 * tpresent: last time tb_present() was called.
 * tcurrent: buffer for gettimeofday(2).
 */
struct timeval tpresent = { 0, 0 };
struct timeval tcurrent = { 0, 0 };

static size_t
link_color(CURLU *u)
{
	char *scheme, color;
	curl_url_get(u, CURLUPART_SCHEME, &scheme, 0);
	if (!scheme || strcmp(scheme, "gemini"))
		color = MIRC_RED;
	if (hist_contains(u) > 0)
		color = MIRC_MAGENTA;
	color = MIRC_BLUE;
	free(scheme);
	return color;
}

const size_t attribs[] = { TB_BOLD, TB_UNDERLINE, TB_REVERSE };
static inline void
set_color(uint32_t *old, uint32_t *new, char *color)
{
	uint32_t col = strtol(color, NULL, 10);

	*old = *new, *new = col;
	if (col < sizeof(mirc_colors))
		*new = mirc_colors[col];

	for (size_t i = 0; i < sizeof(attribs); ++i)
		if ((*old & attribs[i]) == attribs[i])
			*new |= attribs[i];
}

static void
tb_clearline(size_t line, struct tb_cell *c)
{
	int width = tb_width(), col = 0;
	do tb_put_cell(col, line, c); while (++col < width);
}

static void
tb_writeline(size_t line, char *string, size_t skip)
{
	int width = tb_width(), col = 0;
	struct tb_cell c = { ' ', 0, 0 };

	char colorbuf[4] = { '\0', '\0', '\0', '\0' };
	size_t chwidth;
	int32_t charbuf = 0;
	ssize_t runelen = 0;

	uint32_t oldfg = 0, oldbg = 0;

	tb_clearline(line, &c);

	/* restore colors of previous line. */
	c.fg = oldfg, c.bg = oldbg;

	while (*string && col < width) {
		switch (*string) {
		break; case MIRC_BOLD:      ++string; c.fg ^= TB_BOLD;
		break; case MIRC_UNDERLINE: ++string; c.fg ^= TB_UNDERLINE;
		break; case MIRC_INVERT:    ++string; c.fg ^= TB_REVERSE;
		break; case MIRC_RESET:     ++string; c.fg = 15, c.bg = 0;
		break; case MIRC_ITALIC:    ++string; break;
		break; case MIRC_BLINK:     ++string; break;
		break; case MIRC_COLOR:
			++string;
			colorbuf[0] = colorbuf[1] = colorbuf[2] = '\0';

			/* if no digits after MIRC_COLOR, reset */
			if (!isdigit(*string)) {
				c.fg = 15, c.bg = 0;
				break;
			}

			colorbuf[0] = *string;
			if (isdigit(string[1])) colorbuf[1] = *(++string);
			set_color(&oldfg, &c.fg, (char *) &colorbuf);

			++string;

			/* bg color may or may not be present */
			if (*string != ',' || !isdigit(string[1]))
				break;

			colorbuf[0] = *(++string);
			if (isdigit(string[1])) colorbuf[1] = *(++string);
			set_color(&oldbg, &c.bg, (char *) &colorbuf);

			string += 2;
		break; case MIRC_256COLOR:
			++string;
			colorbuf[0] = colorbuf[1] = colorbuf[2] = '\0';
			strncpy((char *) &colorbuf, string, 3);
			set_color(&oldfg, &c.fg, (char *) &colorbuf);
			string += 3;
		break; default:
			charbuf = 0;
			runelen = utf8proc_iterate((const unsigned char *) string,
				-1, (utf8proc_int32_t *) &charbuf);
	
			if (runelen < 0) {
				/* invalid UTF8 codepoint, let's just
				 * move forward and hope for the best */
				++string;
				continue;
			}
	
			assert(charbuf >= 0);
			c.ch = (uint32_t) charbuf;
			string += runelen;
	
			chwidth = utf8proc_charwidth((utf8proc_int32_t) c.ch);

			if (skip > 0) {
				--skip;
				continue;
			}

			if (chwidth > 0) {
				tb_put_cell(col, line, &c);
				col += 1;
			}
		}
	}

	tb_status |= TB_MODIFIED;
}

void
ui_init(void)
{
	assert(gettimeofday(&tpresent, NULL) == 0);
	char *errstrs[] = {
		NULL,
		"termbox: unsupported terminal",
		"termbox: cannot open terminal",
		"termbox: pipe trap error"
	};
	char *err = errstrs[-(tb_init())];
	if (err) die(err);
	tb_status |= TB_ACTIVE;
	tb_select_input_mode(TB_INPUT_ALT|TB_INPUT_MOUSE);
	tb_select_output_mode(TB_OUTPUT_256);

	memset(ui_messagebuf, 0x0, sizeof(ui_messagebuf));
}

/*
 * check if (a) REFRESH time has passed, and (b) if the termbox
 * buffer has been modified; if both those conditions are met, "present"
 * the termbox screen.
 */
void
ui_present(void)
{
	assert(gettimeofday(&tcurrent, NULL) == 0);
	struct timeval diff;
	timersub(&tcurrent, &tpresent, &diff);

	if (!timercmp(&diff, &REFRESH, >=))
		return;
	if ((tb_status & TB_MODIFIED) == TB_MODIFIED)
		tb_present();
}


static char *
format_elem(struct Gemtok *l, char *text, size_t lnk, size_t folded)
{
	if (folded > 1) {
		switch (l->type) {
		break; case GEM_DATA_HEADER1:
			return format("  \x02%s", text);
		break; case GEM_DATA_HEADER2:
			return format("   \x02%s", text);
		break; case GEM_DATA_HEADER3:
			return format("    \x02%s", text);
		break; case GEM_DATA_LIST:
			return format("   %s", text);
		break; case GEM_DATA_QUOTE:
			return format(" > \00314%s", text);
		break; case GEM_DATA_LINK:;
			char attr = l->text ? '\x0f' : '\x1f';
			char *padding = strrep(' ', strlen(format("[%zu]", lnk)));
			return format("%s %c\003%zu%s", padding, attr,
					link_color(l->link_url), text);
		break; case GEM_DATA_TEXT: case GEM_DATA_PREFORMAT: default:
			return text;
		}
	}

	switch (l->type) {
	break; case GEM_DATA_HEADER1:
		return format("# \x02%s", text);
	break; case GEM_DATA_HEADER2:
		return format("## \x02%s", text);
	break; case GEM_DATA_HEADER3:
		return format("### \x02%s", text);
	break; case GEM_DATA_LIST:
		return format(" * %s", text);
	break; case GEM_DATA_QUOTE:
		return format(" > \00314%s", text);
	break; case GEM_DATA_LINK:;
		char attr = l->text ? '\x0f' : '\x1f';
		return format("\x02[%zu]\x0f %c\003%zu%s", lnk, attr,
				link_color(l->link_url), text);
	break; case GEM_DATA_TEXT: case GEM_DATA_PREFORMAT: default:
		return text;
	}
}

void
ui_set_gemdoc(struct Gemdoc *g)
{
	assert(g != NULL);
	ui_vscroll = ui_hscroll = 0;
	ui_doc = g;
	ui_redraw();
}

static size_t
_ui_redraw_rendered_doc(void)
{
	assert(ui_doc != NULL);

	size_t line = 0;
	size_t height = (size_t) tb_height();
	size_t width = (size_t) tb_width();

	size_t links = 0, page_height = 0;
	ssize_t scrollctr = ui_vscroll;
	for (struct lnklist *c = ui_doc->document->next; c; c = c->next) {
		struct Gemtok *l = ((struct Gemtok *)c->data);
		char *text = l->text;
		if (l->type == GEM_DATA_LINK) {
			++links;
			text = l->text ? l->text : l->raw_link_url;
		}

		size_t fold_width = l->type == GEM_DATA_PREFORMAT ?
			strlen(text) : width - 5;
		size_t i = 1;
		struct lnklist *t, *folded = strfold(text, fold_width);
		for (t = folded->next; t; t = t->next, ++i) {
			if (--scrollctr >= 0) continue;
			char *fmt = format_elem(l, (char *) t->data, links, i);
			tb_writeline(line, fmt, ui_hscroll);
			if (++line >= height-3) break;
		}
		page_height += lnklist_len(folded);
		lnklist_free_all(folded);
	}

	return page_height;
}

static size_t
_ui_redraw_raw_doc(void)
{
	size_t height = (size_t) tb_height();
	size_t line = 0, page_height = 0;
	ssize_t scrollctr = ui_vscroll;
	for (struct lnklist *c = ui_doc->rawdoc->next; c; c = c->next) {
		if (--scrollctr >= 0) continue;
		tb_writeline(line, (char *)c->data, ui_hscroll);
		++page_height;
		if (++line >= height-3) break;
	}
	return page_height;
}

size_t
ui_redraw(void)
{
	assert(ui_doc != NULL);
	tb_clear();

	size_t height = (size_t) tb_height();
	size_t width = (size_t) tb_width();

	size_t page_height = 0;
	if (ui_raw_doc) page_height = _ui_redraw_raw_doc();
	else page_height = _ui_redraw_rendered_doc();

	/* add 1 to page_height to prevent div-by-0 errors below */
	page_height = page_height + 1;

	/* statusline */
	char *url, lstatus[128], rstatus[128], *pad;
	curl_url_get(ui_doc->url, CURLUPART_URL, &url, 0);

	strcpy(lstatus, format("%3d%%", (ui_vscroll * 100) / (page_height)));
	strcpy(rstatus, format("%s", url));
	pad = strrep(' ', width - strlen(lstatus) - strlen(rstatus) - 2);

	tb_writeline(height-2, format("\x16 %s%s%s ",
				lstatus, pad, rstatus), 0);

	/* inputline */
	tb_clearline(height-1, &((struct tb_cell){0, 0, 0}));

	if (tbrl_len() > 0) {
		for (size_t i = 0; i < tbrl_len(); ++i) {
			tb_change_cell(i, height-1, tbrl_buf[i], 0, 0);
		}
		tb_set_cursor(height-1, tbrl_cursor);

		ui_message(0, ""); /* remove message if there */
	} else if (strlen(ui_messagebuf) > 0) {
		size_t padwidth = width - strlen(ui_messagebuf) - strlen(DISMISS);
		tb_writeline(height-1, format("%s%s\00314%s",
			ui_messagebuf, strrep(' ', padwidth-1), DISMISS), 0);
	}

	if (tbrl_len() > 0)
		tb_set_cursor(tbrl_cursor, height-1);
	else
		tb_set_cursor(TB_HIDE_CURSOR, TB_HIDE_CURSOR);

	free(url);
	return page_height;
}

void __attribute__((format(printf, 2, 3)))
ui_message(enum UiMessageType type, const char *fmt, ...)
{
	ui_message_type = type;
	memset(ui_messagebuf, 0x0, sizeof(ui_messagebuf));

	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(ui_messagebuf,
			sizeof(ui_messagebuf), fmt, ap);
	assert((size_t) len < sizeof(ui_messagebuf));
	va_end(ap);
}

void
ui_handle(struct tb_event *ev)
{
	if (ev->type == TB_EVENT_KEY && ev->key) {
		switch (ev->key) {
		break; case TB_KEY_ENTER:
			if (strlen(ui_messagebuf) == 0) break;
			memset(ui_messagebuf, 0x0, sizeof(ui_messagebuf));
		break; case TB_KEY_CTRL_U:
			ui_raw_doc = !ui_raw_doc;
		}
	}
}

void
ui_shutdown(void)
{
	if ((tb_status & TB_ACTIVE) == TB_ACTIVE) {
		tb_shutdown();
		tb_status ^= TB_ACTIVE;
	}
}
