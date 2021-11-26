#ifndef __UTIL_H__
#define __UTIL_H__

#include <string.h>
#include <sys/times.h>

#include "owntypedef.h"

#define __min(x,y)									((x)<(y)?(x):(y))
#define __max(x,y)									((x)<(y)?(y):(x))

#define __getHiByte(w)							( ((w)>>8) & 0xff)
#define __getLoByte(w)							( (w) & 0xff )
#define __getSemiOct(w)							(__getHiByte(w)*10 + __getLoByte(w))

#define __bypassIfNull(p)							{ if(!(p)) return; }
#define __goToError()									{ goto error; }
#define __goToErrorIfFalse(c)				{ if(!(c)) __goToError(); }
#define __goToErrorIfFalseLog(c,msg)				do { if(!(c)) { syslog(LOG_ERR,msg); __goToError(); } } while(0)

#define __allocObj(t)					( (t*)__alloc(sizeof(t)) )

#define __packedStruct							__attribute__((packed))
#define __printfFunc(fmt,arg)				__attribute__ ((__format__ (__printf__, fmt, arg)));

#define __isSucc(x)							((x)>=0)
#define __isFail(x)							(!__isSucc(x))

#define __isAssigned(p)						( (p)!=NULL )
#define __isNotAssigned(p)				(!__isAssigned(p))

#define __isTrue(c)								((c)!=0)
#define __isFalse(c)							(!__isTrue(c))

#define __getOffset(p,offset)			( (void*)((size_t)(p)+(offset)) )
#define __getNextPtr(p)						( (void*)((p)+1) )

#define __countOf(x)							( sizeof(x)/sizeof((x)[0]) )
#define __lastOf(x)								( __countOf(x)-1 )

#define __forEach(i,p,a)					for(i=0,p=&(a)[i];i<__countOf(a);p=&(a)[++i])
#define __typeOf(i)								typeof(i)
#define __pointTypeOf(i)					__typeOf(i)*
#define __offsetMember(t,m)				(int)(&((t*)NULL)->m)
#define __zeroMem(p,len)					memset(p,0,len)
#define __zeroObj(pObj)						__zeroMem(pObj,sizeof(*pObj))

#define __getStringBoolean(b)			((b)?"True":"False")

#define __getRanged(v,count)			((v)<(count)?(v):(count-1))

void* __alloc(int cbLen);
void __free(void* pMem);

BOOL utils_sleep(int nMS);

void utils_strSNCpy(char* lpszDst, const char* lpszSrc, int cbSrc);
void utils_strDNCpy(char* lpszDst, int cbDst, const char* lpszSrc);
BOOL utils_strTrimCpy(char* szDst, const char* szSrc, int cbSrc);
void utils_strTailTrim(char* szDst);

void utils_strConv2Bytes(char* lpszDest, const char* p2ByteSrc, int c2ByteSrc);

clock_t __getTickCount(void);
clock_t __getTicksPerSecond(void);

long long __htonll(long long l64H);
long long __ntohll(long long l64N);

#endif
