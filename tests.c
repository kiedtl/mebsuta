#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>

#include "util.h"

static size_t __ASSERTIONS = 0;

typedef struct ErrorInfo {
	const char *msg, *file, *fn;
	size_t line;
} error_t;

#define TEST(N, B) \
	static _Bool test_##N(error_t *e) { B; return true; }

#define __ASSERT(EXPR, MSG, FI, FN, LI) do { \
	++__ASSERTIONS; \
	if (!(EXPR)) { \
		e->msg = MSG, e->file = FI, e->fn = FN, e->line = LI; \
		return false; \
	} \
} while (0);

#define ASSERT_T(EXPR) \
	__ASSERT(EXPR, #EXPR" != true", __FILE__, __func__, __LINE__);
#define ASSERT_EQ(A, B) \
	__ASSERT((A) == (B), #A" != "#B, __FILE__, __func__, __LINE__);
#define ASSERT_ZSTR_EQ(STR1, STR2) \
	__ASSERT(!strcmp(STR1, STR2), #STR1" != "#STR2, \
			__FILE__, __func__, __LINE__);

TEST(strrep1, {
	ASSERT_ZSTR_EQ(strrep(' ', 0), "");
})

TEST(strrep2, {
	ASSERT_ZSTR_EQ(strrep(' ', 9), "         ");
})

TEST(stroverlap, {
	ASSERT_EQ(stroverlap("conf", "confuzzle"), 4);
	ASSERT_EQ(stroverlap("QAnon", "intelligence"), 0);
})

TEST(eat, {
	ASSERT_ZSTR_EQ(eat("12345", isdigit, 3), "45");
	ASSERT_ZSTR_EQ(eat("12345", isdigit, 100), "");
	ASSERT_ZSTR_EQ(eat("    t", isblank, 3), " t");
})

TEST(strfold, {
	char *garbage = "I'd like to interject for a moment. What you're referring to as Linux, is in fact GNU/Linux, or as I've recently taken to calling it, GNU plus Linux. Linux is not an operating system unto itself, but rather another free component of a fully functional GNU system make useful by the GNU corelibs, shell utilities and vital components comprising a full OS as defined by POSIX.";
	struct lnklist *r = strfold(garbage, 40);

	/* TODO: write a func to reverse the linked list and use it
	 * instead of comparing the results of lnklist_pop backward */
	ASSERT_EQ(lnklist_len(r), 11);
	ASSERT_ZSTR_EQ(lnklist_pop(r), "POSIX.");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "comprising a full OS as defined by");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "shell utilities and vital components");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "system make useful by the GNU corelibs,");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "component of a fully functional GNU");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "itself, but rather another free");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "Linux is not an operating system unto");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "taken to calling it, GNU plus Linux.");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "in fact GNU/Linux, or as I've recently");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "What you're referring to as Linux, is");
	ASSERT_ZSTR_EQ(lnklist_pop(r), "I'd like to interject for a moment.");

	char *beginning_spaces = "          Then they  started  from  their places,      moved with violence, changed in hue.";
	struct lnklist *r1 = strfold(beginning_spaces, 40);
	ASSERT_EQ(lnklist_len(r1), 2);
	ASSERT_ZSTR_EQ(lnklist_pop(r1), "moved with violence, changed in hue.");
	ASSERT_ZSTR_EQ(lnklist_pop(r1), "Then they  started  from  their places,");
})

int
main(void)
{
	_Bool (*const testfuncs[])(error_t *e) = {
		test_strrep1, test_strrep2, test_stroverlap, test_eat,
		test_strfold
	};

	error_t errors[SIZEOF(testfuncs)];
	size_t errorctr = 0;

	struct timeval begin, end, duration;

	gettimeofday(&begin, NULL);

	printf("\n");
	printf("-- Starting test suite (%zu test(s))\n", SIZEOF(testfuncs));
	printf("    ");

	for (size_t i = 0; i < SIZEOF(testfuncs); ++i) {
		_Bool r = (testfuncs[i])(&errors[errorctr]);

		if (!r)
			++errorctr;

		putchar(r ? '.' : 'E');
	}

	gettimeofday(&end, NULL);
	timersub(&end, &begin, &duration);
	printf("\n-- Testing finished in %ld.%02lds (%zu assertions)\n",
		duration.tv_sec, duration.tv_usec, __ASSERTIONS);
	printf("    %zu tests, %zu passed, %zu failed.\n",
		SIZEOF(testfuncs), SIZEOF(testfuncs)-errorctr, errorctr);

	if (errorctr > 0) {
		printf("\nErrors:\n");
		for (size_t e = 0; e < errorctr; ++e) {
			printf("    · \x1b[1m%s\x1b[m(%s:%zu): %s\n", errors[e].fn,
				errors[e].file, errors[e].line, errors[e].msg);
		}
	}

	printf("\n");
}
