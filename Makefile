CMD      = @

VERSION  = 0.1.0
NAME     = mebs
SRC      = main.c util.c conn.c list.c gemini.c history.c ui.c \
	   tbrl.c mirc.c \
	   strlcpy.c curl/url.c curl/escape.c
OBJ      = $(SRC:.c=.o)

TERMBOX  = tb/bin/termbox.a
UTF8PROC = ~/local/lib/libutf8proc.a

WARNING  = -Wall -Wpedantic -Wextra -Wold-style-definition \
	   -Wmissing-prototypes -Winit-self -Wfloat-equal -Wstrict-prototypes \
	   -Wredundant-decls -Wendif-labels -Wstrict-aliasing=2 -Woverflow \
	   -Wformat=2 -Wmissing-include-dirs -Wtrigraphs -Wno-format-nonliteral \
	   -Wincompatible-pointer-types -Wunused-parameter \
	   -Werror=extern-initializer
DEF      =
INCL     = -Itb/src/ -I ~/local/include -Icurl/
CC       = clang
CFLAGS   = -Og -g $(DEF) $(INCL) $(WARNING) -funsigned-char
LD       = bfd
LDFLAGS  = -fuse-ld=$(LD) -L/usr/include -lm -ltls

.PHONY: all
all: $(NAME)

.PHONY: run
run: $(NAME)
	$(CMD)./$(NAME)

.c.o: $(HDR)
	@printf "    %-8s%s\n" "CC" $@
	$(CMD)$(CC) -c $< -o $(<:.c=.o) $(CFLAGS)

main.o: commands.c

$(NAME): $(OBJ) $(UTF8PROC) $(TERMBOX)
	@printf "    %-8s%s\n" "CCLD" $@
	$(CMD)$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(TERMBOX):
	@printf "    %-8s%s\n" "MAKE" $@
	$(CMD)make -C tb CC=$(CC)

.PHONY: clean
clean:
	rm -f $(NAME) $(OBJ)
