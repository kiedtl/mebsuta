#ifndef UI_H
#define UI_H

#include "gemini.h"
#include "termbox.h"

enum UiMessageType {
	UI_MSG_WARN = 1,
	UI_MSG_INFO = 2,
	UI_MSG_STOP = 3
};

extern char ui_message[255];
extern enum UiMessageType ui_message_type;

extern size_t tb_status;
extern const size_t TB_ACTIVE, TB_MODIFIED;
extern size_t ui_vscroll, ui_hscroll;
extern struct Gemdoc *ui_doc;

extern _Bool ui_raw_doc;

void ui_init(void);
void ui_present(void);
void ui_set_gemdoc(struct Gemdoc *g);
size_t ui_redraw(void);
void ui_handle(struct tb_event *ev);
void ui_shutdown(void);

#endif
