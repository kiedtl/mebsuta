#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include "list.h"

#define UNUSED(VAR) ((void) (VAR))
#define SIZEOF(ARR) (sizeof(ARR)/sizeof(*(ARR)))

void die(const char *fmt, ...);
char *format(const char *format, ...);
char *strrep(char c, size_t n);

struct lnklist *strfold(char *str, size_t width);

#endif
