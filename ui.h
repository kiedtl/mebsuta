#ifndef UI_H
#define UI_H

#include "gemini.h"

extern size_t tb_status;
extern const size_t TB_ACTIVE, TB_MODIFIED;
extern size_t ui_vscroll, ui_hscroll;
extern struct Gemdoc *ui_doc;

void ui_init(void);
void ui_set_gemdoc(struct Gemdoc *g);
void ui_present(void);
void ui_display_gemdoc(void);
void ui_shutdown(void);

#endif
