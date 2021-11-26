#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "minilib.h"

#define LINE_SEPERATOR "====================================================================="

static char _last_error[1024]={0,};
static clock_t clk_per_sec=0;

void bp(void)
{
}

int __strtok(const char* str,const char* delimiters,char* qutations,int index,char* buf,int buf_len)
{
	int i;

	int qutation;
	int in_qutation;

	char ch;

	int ret;

	ret=-1;

	in_qutation=0;
	qutation=0;

	i=0;
	while((ch=*str++)!=0) {

		if(qutations)
			qutation=strchr(qutations,ch)!=NULL;

		// check start qutation
		if(qutation) {
			in_qutation=!in_qutation;
		}
		else {
			// increase index if delimiter
			if(!in_qutation && strchr(delimiters,ch)) {
				i++;
			}
			else if(i==index) {
				if(buf_len>1) {
					*buf++=ch;
					buf_len--;
				}

				ret=0;
			}
		}
	}

	*buf=0;

	return ret;
}

unsigned int __hex(char hex)
{
	unsigned char result;

	if(hex>='A' && hex<='F')
		result = hex-('A'-10);
	else if(hex>='a' && hex<='f')
		result = hex-('a'-10);
	else
		result = hex - '0';

	return result;
}

inline char* __strncpy(char* dst,const char* src,int len)
{
	if(len>1)
		strncpy(dst,src,len-1);

	if(len>0)
		dst[len-1]=0;

	return dst;
}

void _set_lasterror(const char* fmt,...)
{
	va_list ap;

	if(fmt) {
		va_start(ap, fmt);
		vsnprintf(_last_error,sizeof(_last_error),fmt,ap);
		va_end(ap);
	}
	else {
		_last_error[0]=0;
	}
}

static void _print_lasterror(const char* func,int line,const char* fmt,va_list ap)
{
	fprintf(stderr,"error dump\n");
	fprintf(stderr,"%s\n",LINE_SEPERATOR);
	fprintf(stderr,"module     \t : %s\n",func);
	fprintf(stderr,"line       \t : %d\n",line);
	fprintf(stderr,"error      \t : %d - %s\n",errno,strerror(errno));

	if(_last_error[0])
		fprintf(stderr,"last error \t : %s\n",_last_error);

	if(fmt) {
		fprintf(stderr,"desc       \t : ");
		vfprintf(stderr,fmt,ap);
		fprintf(stderr,"\n");
	}

	fprintf(stderr,"\n");
}

void _critical(const char* func,int line,const char* fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	_print_lasterror(func,line,fmt,ap);
	va_end(ap);
}

void _fatal(const char* func,int line,const char* fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	_print_lasterror(func,line,fmt,ap);
	va_end(ap);

	exit(-1);
}

void* _malloc(int len)
{
	void* p;

	p=malloc(len);
	if(p)
		memset(p,0,len);

	return p;
}

void _free(void* p)
{
	if(p)
		free(p);
}

static int _dump_int(int level,const char* dump_name,void* p, int len,char* buf,int buflen)
{
	char* p2;
	int i;
	char ch;

	if(len>16) {
		SYSLOG(level,"too big source - len=%d",len);
		goto err;
	}

	if(buflen<16*(2+1)+16+1) {
		SYSLOG(level,"insufficient buffer length - buflen=%d",buflen);
		goto err;
	}

	// put prefix
	sprintf(buf,"[%s] ",dump_name);
	buf+=strlen(buf);

	// print hex
	i=0;
	p2=p;
	while(i<16) {
		if(i++<len) {
			sprintf(buf,"%02X ",(unsigned char)*p2++);
		}
		else {
			sprintf(buf,"   ");
		}

		buf+=3;
	}

	// print ascii
	i=0;
	p2=p;
	while(i++<len) {
		ch=*p2++;

		if(!isprint(ch) || iscntrl(ch) || !ch)
			ch='.';

		sprintf(buf,"%c",ch);
		buf++;
	}

	return 0;
err:
	return -1;
}

void _print_hexs(FILE* fd, const char* buf,int len)
{
	int i=0;

	i=0;
	while(i++<len) {
		if(i)
			fprintf(fd," ");
		else
			fprintf(fd,"%02x",*buf++);
	}
}

void _dump(int level,const char* dump_name,void* p,int len)
{
	char* p2;
	char line[1024];

	p2=(char*)p;
	while(len>0) {
		_dump_int(level,dump_name,p2,(len<16)?len:16,line,sizeof(line));
		len-=16;
		p2+=16;

		syslog(level,"%s",line);
	}
}

void minilib_init(void)
{
	clk_per_sec=sysconf(_SC_CLK_TCK);
	if(clk_per_sec<=0)
		_fatal(__FUNCTION__,__LINE__,"failed in sysconf()");
}

clock_t _get_current_sec(void)
{
	clock_t clk;
	struct tms tmsbuf;

	clk=times(&tmsbuf);

	return clk/clk_per_sec;
}

// return 1 if the whole string is printable
int is_printable(const char* s, int len)
{
    int i;
    for (i=0; i < len; i++)
    {
	if (!isprint(s[i])) {
	    return 0;
	}
    }
    return 1;
}
