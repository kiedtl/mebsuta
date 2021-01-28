#ifndef UI_H
#define UI_H

#include "history.h"
#include "gemini.h"
#include "termbox.h"

#define UI_BOLD       '\x02'
#define UI_UNDERLINE  '\x1f'
#define UI_ITALIC     '\x1d'
#define UI_INVERT     '\x16'
#define UI_BLINK      '\x06'
#define UI_RESET      '\x0f'
#define UI_COLOR      '\x03'

enum UiMessageType {
	UI_WARN = 1,
	UI_INFO = 2,
	UI_STOP = 3
};

enum UiDocumentMode {
	UI_DOCNORM    = (1<<1),
	UI_DOCRAW     = (1<<3),
	UI_DOCRAWLINK = (1<<2)
};

void ui_init(void);
void ui_present(void);
void ui_set_gemdoc(struct Gemdoc *g);
size_t ui_redraw(void);
void ui_message(enum UiMessageType type, const char *fmt, ...);
void ui_handle(struct tb_event *ev);
void ui_shutdown(void);

#endif
