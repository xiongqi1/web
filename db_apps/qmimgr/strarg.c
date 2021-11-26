#include "strarg.h"

#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#define STRARG_DELIM	" "

void strarg_destroy(struct strarg_t* a)
{
	if(!a)
		return;

	free(a->arg);
	free(a);
}

struct strarg_t* strarg_create(const char* arg) {
	struct strarg_t* a;
	char* token;

	char* sp;
	int i;

	if(!(a = calloc(1, sizeof(*a)))) {
		syslog(LOG_DEBUG, "###channel### cannot create strarg");
		goto err;
	}

	if(!(a->arg = strdup(arg))) {
		syslog(LOG_DEBUG, "###channel### cannot duplicate string (arg=%s)", arg);
		goto err;
	}

	/* break up tokens */
	i = 0;
	token = strtok_r(a->arg, STRARG_DELIM, &sp);
	while(token) {
		a->argv[i++] = token;
		a->argc = i;

		token = strtok_r(NULL, STRARG_DELIM, &sp);
	}

	return a;

err:
	strarg_destroy(a);
	return NULL;
}

