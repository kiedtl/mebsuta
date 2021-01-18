#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include "list.h"

#define MAX(V,M)     ((V)>(M)?(M):(V))
#define MIN(V,M)     ((V)<(M)?(M):(V))
#define CLAMP(V,H,L) (MIN(MAX(V,H),L))
#define CHKSUB(A,B)  (((A)-(B))>(A)?0:((A)-(B)))
#define UNUSED(VAR)  ((void) (VAR))
#define SIZEOF(ARR)  (sizeof(ARR)/sizeof(*(ARR)))
#define BITSET(V,B)  (((V) & (B)) == (B))

void die(const char *fmt, ...);
char *format(const char *format, ...);
char *strrep(char c, size_t n);

struct lnklist *strfold(char *str, size_t width);

#endif
