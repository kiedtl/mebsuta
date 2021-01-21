#ifndef UI_H
#define UI_H

#include "history.h"
#include "gemini.h"
#include "termbox.h"

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

extern size_t tb_status;
extern const size_t TB_ACTIVE, TB_MODIFIED;

void ui_init(void);
void ui_present(void);
void ui_set_gemdoc(struct Gemdoc *g);
size_t ui_redraw(void);
void ui_message(enum UiMessageType type, const char *fmt, ...);
void ui_handle(struct tb_event *ev);
void ui_shutdown(void);

#endif
