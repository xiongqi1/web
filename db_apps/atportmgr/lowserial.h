#ifndef __LOWSERIAL_H__
#define __LOWSERIAL_H__

#include <termios.h>
#include <linux/limits.h>

#include "atcmdqueue.h"

typedef enum
{
	lowserial_result_ok,
	lowserial_result_error,
	lowserial_result_unknown
} lowserial_result;

typedef struct
{
	int hDev;							// AT port handle

	char* pConvBuf;
	int cbConvBuf;

	char* pAtCmdResult;		// AT command answer buffer to hand over
	int cbAtCmdResult;		// length of AT command answer buffer

	atcmdqueue* pRQ;			// serial receive buffer
	atcmdqueue* pWQ;			// serial transfer buffer

	int fOldTermIo;
	struct termios oldTermIo;

	char achDevName[PATH_MAX];

} lowserial;

void lowserial_destroy(lowserial* pS);
const char* lowserial_read(lowserial* pS);
int lowserial_gather(lowserial* pS, lowserial_result* pRes);
int lowserial_write(lowserial* pS, const void* pToWrite, int cbToWrite);
lowserial* lowserial_create(const char* szDev, int cbQ, int cbAtCmdResult);
int lowserial_getDev(lowserial* pS);
void lowserial_clearReadQueue(lowserial* pS);
int lowserial_flushWrite(lowserial* pS);

int lowserial_close(lowserial* pS);
int lowserial_open(lowserial* pS);
int lowserial_isOpen(lowserial* pS);

int lowserial_eatGarbage(lowserial* pS);
const char* lowserial_getDevName(lowserial* pS);
int lowserial_getWriteQueueLen(lowserial* pS);

#endif
