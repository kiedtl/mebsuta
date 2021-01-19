#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "tbrl.h"
#include "termbox.h"
#include "utf8proc.h"
#include "util.h"

void (*tbrl_enter_callback)(char *buf) = NULL;
void (*tbrl_complete_callback)(char *buf, size_t curs, char *completebuf) = NULL;

size_t tbrl_cursor = 0;
uint32_t tbrl_buf[TBRL_BUFSIZE];
char tbrl_hint[TBRL_BUFSIZE];

static void
tbrl_reset(void)
{
	tbrl_cursor = 0;
	memset(tbrl_buf, 0x0, TBRL_BUFSIZE);
}

void
tbrl_init(void)
{
	tbrl_reset();
}

size_t
tbrl_len(void)
{
	for (size_t i = 0; i < TBRL_BUFSIZE; ++i)
		if (tbrl_buf[i] == 0) return i;
	return sizeof(tbrl_buf);
}

void
tbrl_handle(struct tb_event *ev)
{
	if (ev->type == TB_EVENT_KEY && ev->key) {
		switch (ev->key) {
		break; case TB_KEY_HOME: case TB_KEY_CTRL_A:
			tbrl_cursor = 1; /* move cursor to just after : */
		break; case TB_KEY_END:  case TB_KEY_CTRL_E:
			tbrl_cursor = tbrl_len();
		break; case TB_KEY_CTRL_K:
			memset(&tbrl_buf[tbrl_cursor], 0x0, tbrl_len() - tbrl_cursor);
		break; case TB_KEY_CTRL_U:
			if (tbrl_cursor == 1)
				return;
			size_t len = tbrl_len() - tbrl_cursor + 1;
			/* copy one extra byte in order to add a trailing 0x0.
			 * this saves a memset call. */
			memmove(&tbrl_buf[1], &tbrl_buf[tbrl_cursor],
					(len * CHARSZ + CHARSZ) + 1);
			tbrl_cursor = 1;
		break; case TB_KEY_CTRL_W:;
			size_t word = tbrl_buf[tbrl_cursor] ? tbrl_cursor : tbrl_cursor-1;
			while (utf8isblank(tbrl_buf[word])  && word > 1) --word;
			while (!utf8isblank(tbrl_buf[word]) && word > 1) --word;
			memmove(&tbrl_buf[word], &tbrl_buf[tbrl_cursor],
					(tbrl_cursor - word) * CHARSZ + CHARSZ + 1);
			tbrl_cursor = word;
		break; case TB_KEY_ARROW_LEFT:  case TB_KEY_CTRL_B:
			if (tbrl_cursor > 0) --tbrl_cursor;
		break; case TB_KEY_ARROW_RIGHT: case TB_KEY_CTRL_F:
			if (tbrl_cursor < tbrl_len()) ++tbrl_cursor;
		break; case TB_KEY_DELETE:
			if ((tbrl_cursor+1) <= tbrl_len()) {
				memmove(&tbrl_buf[tbrl_cursor], &tbrl_buf[tbrl_cursor+1],
						(tbrl_len() - tbrl_cursor) * CHARSZ + CHARSZ);
			}
		break; case TB_KEY_BACKSPACE2: case TB_KEY_BACKSPACE:
			if ((tbrl_cursor+1) <= tbrl_len()) {
				memmove(&tbrl_buf[tbrl_cursor-1], &tbrl_buf[tbrl_cursor],
						(tbrl_len() - tbrl_cursor) * CHARSZ + CHARSZ);
			} else {
				tbrl_buf[tbrl_cursor-1] = '\0';
			}
			--tbrl_cursor;
		break; case TB_KEY_ENTER:;
			char chbuf[TBRL_BUFSIZE*6];
			utf8encode(tbrl_buf, tbrl_len(), chbuf, SIZEOF(chbuf));
			tbrl_reset();
			(tbrl_enter_callback)((char *)chbuf);
		break; case TB_KEY_CTRL_G: case TB_KEY_CTRL_C:
			tbrl_reset();
		break; case TB_KEY_SPACE:
			tbrl_handle(&((struct tb_event)
				{ TB_EVENT_KEY, 0, 0, ' ', 0, 0, 0, 0 }));
		}
	} else if (ev->type == TB_EVENT_KEY && ev->ch) {
		assert(ev->ch != '\0');

		/* only show hints if the cursor is at the end */
		memset(tbrl_hint, 0x0, TBRL_BUFSIZE);
		if (tbrl_cursor == (tbrl_len() - 1) && tbrl_complete_callback) {
			char chbuf[TBRL_BUFSIZE*6];
			utf8encode(tbrl_buf, tbrl_len(), chbuf, SIZEOF(chbuf));
			(tbrl_complete_callback)(chbuf, tbrl_cursor, tbrl_hint);
		}


		if ((tbrl_cursor+1) <= tbrl_len()) {
			memmove(&tbrl_buf[tbrl_cursor+1], &tbrl_buf[tbrl_cursor],
					(tbrl_len() - tbrl_cursor) * CHARSZ + CHARSZ);
		}

		tbrl_buf[tbrl_cursor] = ev->ch;
		++tbrl_cursor;
	}

	if (tbrl_buf[0] != ':') tbrl_reset();
}

void
tbrl_setbuf(char *s)
{
	utf8proc_int32_t buf = -1;

	for (size_t codepoint = 0; *s; ++codepoint) {
		s += utf8proc_iterate((unsigned char *)s, strlen(s), &buf);
		if (buf != -1) tbrl_buf[codepoint] = (uint32_t)buf;
	}

	tbrl_cursor = tbrl_len();
}
