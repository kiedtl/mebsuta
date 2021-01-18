#include <stdint.h>
#include <string.h>

#include "termbox.h"

#define CHARSZ (sizeof(uint32_t))
#define TBRL_BUFSIZE 4096

extern void (*tbrl_enter_callback)(char *buf);
extern void (*tbrl_complete_callback)(char *buf, size_t curs, char *completebuf);
extern size_t tbrl_cursor;
extern uint32_t tbrl_buf[TBRL_BUFSIZE];
extern char tbrl_hint[TBRL_BUFSIZE];

void tbrl_init(void);
size_t tbrl_len(void);
void tbrl_handle(struct tb_event *ev);
void tbrl_setbuf(char *s);
