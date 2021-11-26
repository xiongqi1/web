#ifndef SERDEB_H
#define SERDEB_H

#ifndef TEST
#include <avr/pgmspace.h>
#endif

/* Make sure the selected pin is configured as output. */

void s_printf(const char *fmt, ...);
void print_prog(const char *s);

static inline void nodebug(const char *fmt, ...)
{
	(void)fmt;
}

void debugchar(uint8_t v);

/* Ansi sequences */
#define ANSI_CLR "\033[2J"

#define DBG(fmt,...) s_printf(PSTR(fmt), ##__VA_ARGS__)

#ifndef DLEVEL
#define DLEVEL 0
#endif

#if DLEVEL >= 1
	#define DBG1 DBG
#else
	#define DBG1 nodebug
#endif

#if DLEVEL >= 2
	#define DBG2 DBG
#else
	#define DBG2 nodebug
#endif

#ifdef PARANOIA
	#define ASSERT(cond) do { if (!(cond)) { DBG(#cond); while(1); } } while(0)
#else
	#define ASSERT(...) do {} while(0)
#endif

#endif /* SERDEB_H */
