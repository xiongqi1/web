#ifndef __STRARG_H__
#define __STRARG_H__

#define STRARG_ARGMENT_SIZE 32

/* string argument */
struct strarg_t {
	char* arg;

	int argc;
	char* argv[STRARG_ARGMENT_SIZE];
};

/* str arguemnt functions */
struct strarg_t* strarg_create(const char* arg);
void strarg_destroy(struct strarg_t* a);
#endif
