#include "strarg.h"

#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#define STRARG_ESC      '\\'
#define STRARG_DELIM	' '

void strarg_destroy(struct strarg_t* a)
{
	if(!a)
		return;

	free(a->arg);
	free(a);
}

struct strarg_t* strarg_create(const char* arg)
{
	struct strarg_t* a;

	if(!(a = calloc(1, sizeof(*a)))) {
		syslog(LOG_DEBUG, "###channel### cannot create strarg");
		goto err;
	}

	if(!(a->arg = strdup(arg))) {
		syslog(LOG_DEBUG, "###channel### cannot duplicate string (arg=%s)", arg);
		goto err;
	}

	/* broken arguments */
	int di = 0;
	int si = 0;
	int starti = 0;
	int esc = 0;
	int argi = 0;
	char ch;
	while(((ch = arg[si]) != 0) && (di < STRARG_LINEBUFF_SIZE) && (argi < STRARG_ARGMENT_SIZE)) {

		/* if first escape char */
		if(!esc && (ch == STRARG_ESC)) {
			esc = 1;

			si++;
		} else {
			/* if in escape sequence or not delim */
			if(esc || (ch != STRARG_DELIM)) {
				a->line[di++] = ch;
				si++;
			}
			/* if delimi */
			else if(ch == STRARG_DELIM) {
				a->line[di++] = 0;
				si++;

				/* get token */
				a->argv[argi++] = &a->line[starti];
				/* get new start of string */
				starti = di;
			}

			esc = 0;
		}
	}

	/* get left-over token */
	if((starti != di) || !starti) {
		a->line[di++] = 0;
		/* store start of string */
		a->argv[argi++] = &a->line[starti];
	}

	/* get count */
	a->argc = argi;

	return a;

err:
	strarg_destroy(a);
	return NULL;
}

#if 0
#include <stdio.h>

int main(int argc, char argv[])
{
	struct strarg_t* a;
	int i;

	a = strarg_create("a   b c d\\ e\\ f\\\\ abcd ");

	for(i = 0; i < a->argc; i++) {
		printf("%d : '%s'\n", i, a->argv[i]);
	}

	strarg_destroy(a);

}
#endif

