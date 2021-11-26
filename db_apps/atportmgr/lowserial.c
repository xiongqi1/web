#include "lowserial.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "tickcount.h"
#include "atportmgr.h"

///////////////////////////////////////////////////////////////////////////////
static int convNLtoCRNL(const char* pSrc, int cbSrc, char* pDst, int cbDst)
{
	const char* pSrcE = pSrc + cbSrc;
	char* pDstE = pDst + cbDst - 1;
	char* pD = pDst;

	char chSrc;

	while (pSrc < pSrcE && pD < pDstE)
	{
		chSrc = *pSrc++;

		if (chSrc == '\n')
			*pD++ = '\r';

		*pD++ = chSrc;
	}

	return pD -pDst;
}
///////////////////////////////////////////////////////////////////////////////
static int convCRNLtoNL(const char* pSrc, int cbSrc, char* pDst, int cbDst)
{
	const char* pSrcE = pSrc + cbSrc;
	char* pDstE = pDst + cbDst;
	char* pD = pDst;

	char chSrc;

	while (pSrc < pSrcE && pD < pDstE)
	{
		chSrc = *pSrc++;

		if (chSrc != '\r')
			*pD++ = chSrc;
	}

	return pD -pDst;
}
///////////////////////////////////////////////////////////////////////////////
static int lowserial_recoverPort(int hFd, struct termios* pOldTermIo)
{
	int stat;

	stat =  tcflush(hFd, TCIFLUSH);
	if (stat < 0)
		goto error;

	stat = tcsetattr(hFd, TCSANOW, pOldTermIo);
	if (stat < 0)
		goto error;

	return 0;

error:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
static int lowserial_setupPort(int hFd, int nBaudRate, struct termios* pOldTermIo)
{
	int stat;

	/* Save old port settings. */
	stat = tcgetattr(hFd, pOldTermIo);
	if (stat < 0)
		goto error;

	struct termios newtio;
	memcpy(&newtio, pOldTermIo, sizeof(*pOldTermIo));

	cfmakeraw(&newtio);

	newtio.c_cc[VMIN] = 0;
	newtio.c_cc[VTIME] = 0;

	// set baud rate
	cfsetospeed(&newtio, nBaudRate);

	// set
	stat =  tcsetattr(hFd, TCSAFLUSH, &newtio);
	if (stat < 0)
		goto error;

	return 0;

error:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int lowserial_getDev(lowserial* pS)
{
	return pS->hDev;
}
///////////////////////////////////////////////////////////////////////////////
void lowserial_destroy(lowserial* pS)
{
	if (!pS)
		return;

	lowserial_close(pS);

	atcmdqueue_destroy(pS->pRQ);
	atcmdqueue_destroy(pS->pWQ);

	if (pS->pAtCmdResult)
		free(pS->pAtCmdResult);

	if (pS->pConvBuf)
		free(pS->pConvBuf);

	free(pS);
}

///////////////////////////////////////////////////////////////////////////////
void lowserial_clearReadQueue(lowserial* pS)
{
	atcmdqueue_clear(pS->pRQ);
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_gather(lowserial* pS, lowserial_result* pRes)
{
	char achBuf[512];
	int cbRead;
	int cbWritten;

	// read from queue
	cbRead = read(pS->hDev, achBuf, sizeof(achBuf));
	int cbNew = convCRNLtoNL(achBuf, cbRead, pS->pConvBuf, pS->cbConvBuf);

	cbWritten = atcmdqueue_write(pS->pRQ, pS->pConvBuf, cbNew);
	if (cbWritten != cbNew)
		syslog(LOG_ERR, "serial AT command buffer overflowed (cbToWrite=%d,cbWritten=%d)", cbNew, cbWritten);

	lowserial_result res = lowserial_result_unknown;

	int cbResult;

	// look for OK
	if (res == lowserial_result_unknown)
	{
		cbResult = atcmdqueue_seekEoC(pS->pRQ, "\nOK\n");
		if (cbResult > 0)
			res = lowserial_result_ok;
	}

	// look for ERROR
	if (res == lowserial_result_unknown)
	{
		cbResult = atcmdqueue_seekEoC(pS->pRQ, "\nERROR\n");
		if (cbResult > 0)
			res = lowserial_result_error;
	}

	// put result
	if (pRes)
		*pRes = res;

	// more read needed
	if (res == lowserial_result_unknown)
		goto error;

	return cbResult;

error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
const char* lowserial_read(lowserial* pS)
{
	int cbMax = pS->cbAtCmdResult - 1;

	// if too big result
	int cbToRead = atcmdqueue_get_count(pS->pRQ);
	if (cbToRead > cbMax)
	{
		syslog(LOG_ERR, "AT command result is too big (len=%d,max=%d)", cbToRead, cbMax);
		cbToRead = cbMax;
	}

	// read queue
	int cbQRead = atcmdqueue_read(pS->pRQ, pS->pAtCmdResult, cbToRead);
	if (cbQRead != cbToRead)
		syslog(LOG_ERR, "at command queue wierdly broken");

	// put NULL termination
	pS->pAtCmdResult[cbQRead+1] = 0;

	// clear
	atcmdqueue_clear(pS->pRQ);

	return pS->pAtCmdResult;
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_getWriteQueueLen(lowserial* pS)
{
	return atcmdqueue_get_count(pS->pWQ);
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_flushWrite(lowserial* pS)
{
	char achBuf[256];
	int cbPeek;
	int cbWritten;

	int cbTotal = 0;

	atcmdqueue* pQ = pS->pWQ;

	while (1)
	{
		// peek commands
		cbPeek = atcmdqueue_peek(pQ, achBuf, sizeof(achBuf));
		if (!cbPeek)
			break;

		// write into serial
		cbWritten = write(pS->hDev, achBuf, cbPeek);
		if (cbWritten < 0)
		{
			int nErr = errno;

			if (nErr != EAGAIN)
			{
				syslog(LOG_ERR, "fail to write (len=%d,err=%s)", cbPeek, strerror(errno));
				goto error;
			}
		}

		syslog(LOG_DEBUG, "write into serial (queue=%d,written=%d)", cbPeek, cbWritten);

		// waste from queue
		atcmdqueue_waste(pQ, cbWritten);

		cbTotal += cbWritten;
	}

	if (cbTotal)
		fsync(pS->hDev);

	return cbTotal;

error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_write(lowserial* pS, const void* pToWrite, int cbToWrite)
{
	int cbNew = convNLtoCRNL(pToWrite, cbToWrite, pS->pConvBuf, pS->cbConvBuf);

	return atcmdqueue_write(pS->pWQ, pS->pConvBuf, cbNew);
}

///////////////////////////////////////////////////////////////////////////////
int lowserial_isOpen(lowserial* pS)
{
	return !(pS->hDev < 0);
}
///////////////////////////////////////////////////////////////////////////////
const char* lowserial_getDevName(lowserial* pS)
{
	return pS->achDevName;
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_open(lowserial* pS)
{
	// open
	pS->hDev = open(pS->achDevName, O_RDWR | O_NOCTTY | O_TRUNC);
	if (pS->hDev < 0)
	{
		syslog(LOG_ERR, "fail to open serial port (dev=%s,err=%s)", pS->achDevName, strerror(errno));
		goto error;
	}

	// setup serial port
	if (lowserial_setupPort(pS->hDev, B9600, &pS->oldTermIo) < 0)
	{
		syslog(LOG_ERR, "fail to setup serial port (dev=%s,err=%s)", pS->achDevName, strerror(errno));
		goto error;
	}
	pS->fOldTermIo = 1;

	return 0;

error:
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_eatGarbage(lowserial* pS)
{
	struct timeval tv = {0, 0};

	fd_set fdR;

	FD_ZERO(&fdR);
	FD_SET(pS->hDev, &fdR);

	int nFds = pS->hDev + 1;

	char achGarbage[512];

	// clear write queue
	atcmdqueue_clear(pS->pWQ);

	// send at
	const char* szAT = "at\n";
	lowserial_write(pS, szAT, strlen(szAT));

	// flush until over
	int cbWritten;
	while ((cbWritten = lowserial_flushWrite(pS)) > 0);

	// eat garbage
	int stat = 0;
	tick tckS = getTickCountMS();
	while (1)
	{
		stat = select(nFds, &fdR, NULL, NULL, &tv);
		if (stat > 0)
			read(pS->hDev, achGarbage, sizeof(achGarbage));

		tick tckC = getTickCountMS();
		if (tckC > tckS + ATPORTMGR_ATCOMMAND_DELAY)
			break;
	}

	// clear all queue
	atcmdqueue_clear(pS->pRQ);
	atcmdqueue_clear(pS->pWQ);

	return stat;
}
///////////////////////////////////////////////////////////////////////////////
int lowserial_close(lowserial* pS)
{
	if (!(pS->hDev < 0))
	{
		if (pS->fOldTermIo)
			lowserial_recoverPort(pS->hDev, &pS->oldTermIo);

		close(pS->hDev);
	}

	pS->hDev = -1;
	pS->fOldTermIo = 0;

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
lowserial* lowserial_create(const char* szDev, int cbQ, int cbAtCmdResult)
{
	lowserial* pS;

	// allocate object
	pS = malloc(sizeof(*pS));
	if (!pS)
	{
		syslog(LOG_ERR, "failed to allocate memory for a lowserial object");
		goto error;
	}
	memset(pS, 0, sizeof(*pS));

	pS->hDev = -1;

	// store dev name
	strncpy(pS->achDevName, szDev, sizeof(pS->achDevName));
	pS->achDevName[sizeof(pS->achDevName)-1] = 0;

	// allocate at command result buffer
	pS->cbAtCmdResult = cbAtCmdResult;
	pS->pAtCmdResult = (char*)malloc(pS->cbAtCmdResult);
	if (!pS->pAtCmdResult)
	{
		syslog(LOG_ERR, "failed to allocate command result (size=%d)", pS->cbAtCmdResult);
		goto error;
	}

	pS->cbConvBuf = cbAtCmdResult * 2;
	pS->pConvBuf = (char*)malloc(pS->cbConvBuf);
	if (!pS->pConvBuf)
	{
		syslog(LOG_ERR, "failed to allocate convert buffer (size=%d)", pS->cbConvBuf);
		goto error;
	}

	// create queue
	pS->pRQ = atcmdqueue_create(cbQ);
	if (!pS->pRQ)
	{
		syslog(LOG_ERR, "unable to allocate at command recieve queue (queue size=%d)", cbQ);
		goto error;
	}

	// create transfer queue
	pS->pWQ = atcmdqueue_create(cbQ);
	if (!pS->pWQ)
	{
		syslog(LOG_ERR, "unable to allocate at command transfer queue (queue size=%d)", cbQ);
		goto error;
	}

	return pS;

error:
	lowserial_destroy(pS);
	return NULL;
}


