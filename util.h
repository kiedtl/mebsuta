#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include "list.h"

#define UNUSED(VAR) ((void) (VAR))

void die(const char *fmt, ...);
char *format(const char *format, ...);
char *strrep(char c, size_t n);

struct lnklist *strfold(char *str, size_t width);

#endif
