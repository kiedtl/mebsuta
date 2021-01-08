#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <utf8proc.h>

#include "gemini.h"
#include "list.h"
#include "mirc.h"
#include "termbox.h"
#include "ui.h"
#include "util.h"

extern size_t tb_status, TB_ACTIVE, TB_MODIFIED;

size_t ui_scroll = 0;
struct Gemdoc *ui_doc = NULL;

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
tb_writeline(size_t line, char *string)
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
			return format("%s \x1f%c%s", padding, attr, text);
		case GEM_DATA_TEXT:
		case GEM_DATA_PREFORMAT:
		default:
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
		return format("\x02[%zu]\x0f %c%s", lnk, attr, text);
	break;

	case GEM_DATA_TEXT:
	case GEM_DATA_PREFORMAT:
	default:
		return text;
	}
}

void
ui_display_gemdoc(void)
{
	assert(ui_doc != NULL);
	tb_clear();

	size_t line = 0;
	size_t height = (size_t) tb_height();
	size_t width = (size_t) tb_width();

	size_t links = 0;
	ssize_t scrollctr = ui_scroll;
	struct lnklist *c;
	for (c = ui_doc->document->next; c; c = c->next) {
		struct Gemtok *l = ((struct Gemtok *)c->data);
		char *text = l->text;
		if (l->type == GEM_DATA_LINK) {
			++links;
			text = l->text ? l->text : l->raw_link_url;
		}

		size_t i = 1;
		struct lnklist *t, *folded = strfold(text, width);
		for (t = folded->next; t; t = t->next, ++i) {
			if (--scrollctr >= 0) continue;
			tb_writeline(line,
				format_elem(l, (char *) t->data, links, i));
			free(t->data);
			if (++line >= height-2) break;
		}
		lnklist_free(folded);
	}

	char *url, *padding = strrep(' ', width);
	curl_url_get(ui_doc->url, CURLUPART_URL, &url, 0);
	tb_writeline(height-1, format("\x16 %s%s\x0f", url, padding));
}

void
ui_shutdown(void)
{
	if ((tb_status & TB_ACTIVE) == TB_ACTIVE) {
		tb_shutdown();
		tb_status ^= TB_ACTIVE;
	}
}
