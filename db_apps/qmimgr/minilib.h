#ifndef __MINILIB_H__
#define __MINILIB_H__

#include <sys/times.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>

#define __countof(p)		(sizeof(p)/sizeof(p[0]))
#define __packed		__attribute__ ((__packed__))
#define __noop			{} while(0)
#define __max(a,b)		(((a)>(b))?(a):(b))
#define __min(a,b)		(((a)<(b))?(a):(b))
#define __tck_per_sec		(sysconf(_SC_CLK_TCK))

#define __strncpy_with_zero(d,s,len) \
	{ \
		strncpy(d,s,len); \
		d[len-1]=0; \
	} while(0)

#define SYSLOG(level,fmt,...)	syslog(level,"[%s] " fmt,__FUNCTION__,## __VA_ARGS__)

#define LOG_0		0
#define LOG_1		1

/* local error mode */
#define LOG_ERROR	3
/* normal operation mode - normal operation mode - one above error so we do not pollute the log */
#define LOG_OPERATION	7
/* db activities */
#define LOG_DB		7
/* qmi activities */
#define LOG_COMM	7
/* qmi dump */
#define LOG_DUMP	7
/* all */
#define LOG_DEBUG	7	

#define __UNUSED(x)		(void)x

void minilib_init(void);

clock_t _get_current_sec(void);

void _dump(int level,const char* dump_name,void* p,int len);
void _print_hexs(FILE* fd, const char* buf,int len);


// std functions
void* _malloc(int len);
void _free(void* p);

// inline functions
inline char* __strncpy(char* dst,const char* src,int len);
unsigned int __hex(char hex);

int __strtok(const char* str,const char* delimiters,char* qutations,int index,char* buf,int buf_len);

void bp(void);

int is_printable(const char* s, int len);

#endif
