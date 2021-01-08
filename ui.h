#ifndef UI_H
#define UI_H

#include "gemini.h"

extern size_t ui_scroll; /* TODO: horiz scroll */
extern struct Gemdoc *ui_doc;

void ui_init(void);
void ui_display_gemdoc(void);
void ui_shutdown(void);

#endif
