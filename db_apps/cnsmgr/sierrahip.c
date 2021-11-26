#include "sierrahip.h"

#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <arpa/inet.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int sierrahip_getHandle(sierrahip* pHip)
{
	return pHip->hHipDev;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierrahip_isWriteBufEmpty(sierrahip* pHip)
{
	return binqueue_isEmpty(pHip->pWriteQ);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierrahip_onWrite(sierrahip* pHip)
{
	char achOutBuf[SIERRAHIP_MAX_PAYLOAD_LEN];
	int cbRead;
	int cbWritten;

	while (TRUE)
	{
		// get data to send
		cbRead = binqueue_peek(pHip->pWriteQ, achOutBuf, sizeof(achOutBuf));
		if (!cbRead)
			break;

		// send and waste queue
		cbWritten = write(pHip->hHipDev, achOutBuf, cbRead);
		binqueue_waste(pHip->pWriteQ, cbWritten);

		// do it next time
		if (cbWritten != cbRead)
			break;
	}

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int sierrahip_onRead(sierrahip* pHip)
{
	int cbRead;
	char chRead;

	char* pRead;

	int cbHipPck;

	int cbTotalRead=0;


	char* pLimit = pHip->pHipReadBuf + SIERRAHIP_MAX_HIP_LEN;

	// read
	while (TRUE)
	{
		cbRead = read(pHip->hHipDev, pHip->pHipReadTmpBuf, SIERRAHIP_READ_BUF_LEN);
		if (cbRead == 0)
			break;

		cbTotalRead+=cbRead;

		pRead = pHip->pHipReadTmpBuf;

		// start parsing
		while (cbRead--)
		{
			if ((chRead = *pRead++) == SIERRAHIP_PACKET_STX)
			{
				// change stage
				pHip->parseStage = sierrahip_parse_stage_stx;

				if (pLimit > pHip->pHipReadBufPtr)
				{
					// store
					*pHip->pHipReadBufPtr++ = chRead;

					// if proper size
					if ((cbHipPck = pHip->pHipReadBufPtr - pHip->pHipReadBuf) >= SIERRAHIP_MIN_HIP_LEN)
					{
						SIERRAHIP_HIPHANDLER* lpfnHipHandler;
						sierrahip_hdr* pHipHdr = (sierrahip_hdr*)pHip->pHipReadBuf;
						int cbPayLoadLen = ntohs(pHipHdr->wPayLoadLen);

						int cbExpSize = cbPayLoadLen + SIERRAHIP_MIN_HIP_LEN;

						if (__isAssigned(lpfnHipHandler = (SIERRAHIP_HIPHANDLER*)pHip->lpfnHipHandler) && cbHipPck == cbExpSize)
						{
							sierrahip_tail* pHipTail = (sierrahip_tail*)__getOffset(pHipHdr, sizeof(sierrahip_hdr) + cbPayLoadLen);
							void* pPayLoad = cbPayLoadLen ? __getNextPtr(pHipHdr) : NULL;

							if (pHipHdr->bFramingHdr == SIERRAHIP_PACKET_STX && pHipTail->bFramingTail == SIERRAHIP_PACKET_STX)
								lpfnHipHandler(pHip, pHipHdr->bMsgId, pHipHdr->bParam, pPayLoad, cbPayLoadLen, pHip->pRef);
						}
					}
				}

				// rewind
				pHip->pHipReadBufPtr = pHip->pHipReadBuf;
			}
			else
			{
				// ignore if not proper stage
				if (pHip->parseStage == sierrahip_parse_stage_none)
					continue;

				if (pLimit > pHip->pHipReadBufPtr)
				{
					// put STX if first body
					if (pHip->parseStage == sierrahip_parse_stage_stx)
						*pHip->pHipReadBufPtr++ = SIERRAHIP_PACKET_STX;

					// process if in esc sequence
					if (pHip->parseStage == sierrahip_parse_stage_esc)
					{
						if (chRead == SIERRAHIP_PACKET_ESC_STX)
							chRead = SIERRAHIP_PACKET_STX;
						else if (chRead == SIERRAHIP_PACKET_ESC_ESC)
							chRead = SIERRAHIP_PACKET_ESC;
						else
							continue;
					}
					// change to esc stage if not in esc sequence
					else if (chRead == SIERRAHIP_PACKET_ESC)
					{
						pHip->parseStage = sierrahip_parse_stage_esc;
						continue;
					}

					// store
					*pHip->pHipReadBufPtr++ = chRead;

					pHip->parseStage = sierrahip_parse_stage_body;
				}
			}
		}
	}

	return cbTotalRead;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierrahip_wasteUntilEmpty(sierrahip* pHip)
{
	char achDummy[256];
	int cbRead = -1;

	while (cbRead)
		cbRead = read(pHip->hHipDev, achDummy, sizeof(achDummy));
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierrahip_openHipDev(sierrahip* pHip, const char* lpszHipDev)
{
	struct termios termiosNew;
	struct termios* pTermioOld = &pHip->termiosOld;

	// open
	if (__isFail(pHip->hHipDev = open(lpszHipDev, O_RDWR | O_NOCTTY)))
		return FALSE;

	// set base termio for nes termio config
	if (!(pHip->fTermioOld = __isSucc(tcgetattr(pHip->hHipDev, pTermioOld))))
		__zeroObj(pTermioOld);
	else
		memcpy(&termiosNew, pTermioOld, sizeof(struct termios));

	cfmakeraw(&termiosNew);
	cfsetspeed(&termiosNew, B19200);

	termiosNew.c_cc[VMIN] = 0;
	termiosNew.c_cc[VTIME] = 0;

	// set config
	tcsetattr(pHip->hHipDev, TCSANOW, &termiosNew);

	// waste
	sierrahip_wasteUntilEmpty(pHip);
	tcflush(pHip->hHipDev, TCIOFLUSH);

	// reset parse stage
	pHip->parseStage = sierrahip_parse_stage_none;
	pHip->pHipReadBufPtr = pHip->pHipReadBuf;

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierrahip_closeHipDev(sierrahip* pHip)
{
	int hHipDev;

	// bypass if not open
	if (__isFail(hHipDev = pHip->hHipDev))
		return;

	// restore if old termios existing
	if (pHip->fTermioOld)
		tcsetattr(hHipDev, TCSANOW, &pHip->termiosOld);

	// close
	close(hHipDev);

	// reset flags
	pHip->fTermioOld = FALSE;
	pHip->hHipDev = -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierrahip_writeDummyHip(sierrahip* pHip)
{
	char achDummy[] = {SIERRAHIP_PACKET_STX, SIERRAHIP_PACKET_STX};
	int cbWritten = binqueue_write(pHip->pWriteQ, achDummy, sizeof(achDummy));

	return cbWritten == sizeof(achDummy);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int sierrahip_hipEncode(void* pSrc, int cbSrc, void *pDst, int cbDst)
{
	char* pDstPl = (char*)pDst;
	char* pOrgDstPl = (char*)pDst;
	char* pSrcPl = (char*)pSrc;

	// build hip body
	while (cbSrc--)
	{
		if (*pSrcPl == SIERRAHIP_PACKET_STX)
		{
			*pDstPl++ = SIERRAHIP_PACKET_ESC;
			*pDstPl++ = SIERRAHIP_PACKET_ESC_STX;
		}
		else if (*pSrcPl == SIERRAHIP_PACKET_ESC)
		{
			*pDstPl++ = SIERRAHIP_PACKET_ESC;
			*pDstPl++ = SIERRAHIP_PACKET_ESC_ESC;
		}
		else
		{
			*pDstPl++ = *pSrcPl;
		}

		pSrcPl++;

		if (!(pOrgDstPl + cbDst > pDstPl))
			return -1;
	}

	return (int)(pDstPl -pOrgDstPl);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL sierrahip_writeHip(sierrahip* pHip, unsigned char bMsgId, unsigned char bParam, void* pPayLoad, int cbPayLoad)
{
	char* pDst = pHip->pHipWriteBuf;
	int cbDst = SIERRAHIP_MAX_PAYLOAD_LEN * 2;

	// build hip header
	sierrahip_hdr hipHdr;
	hipHdr.bFramingHdr = SIERRAHIP_PACKET_STX;
	hipHdr.bMsgId = bMsgId;
	hipHdr.bParam = bParam;
	hipHdr.wPayLoadLen = htons(cbPayLoad);

	// STX
	*pDst++ = SIERRAHIP_PACKET_STX;
	cbDst--;

	// encode header
	void* pHipHdrWithoutSTX = &hipHdr.wPayLoadLen;
	int cbHipHdrWithoutSTX = sizeof(hipHdr) - sizeof(hipHdr.bFramingHdr);
	int cbHdrWritten = sierrahip_hipEncode(pHipHdrWithoutSTX, cbHipHdrWithoutSTX, pDst, cbDst);
	if (cbHdrWritten < 0)
	{
		fprintf(stderr, "HIP packet size too big for HIP header\n");
		__goToError();
	}

	// inc.
	pDst += cbHdrWritten;
	cbDst -= cbHdrWritten;

	// encode payload
	int cbPayloadWritten = sierrahip_hipEncode(pPayLoad, cbPayLoad, pDst, cbDst);
	if (cbPayloadWritten < 0)
	{
		fprintf(stderr, "HIP packet size too big for HIP payload\n");
		__goToError();
	}

	// inc.
	pDst += cbPayloadWritten;
	cbDst -= cbPayloadWritten;

	if (cbDst <= 0)
	{
		fprintf(stderr, "HIP packet size too big for ETX\n");
		__goToError();
	}

	// ETX
	*pDst++ = SIERRAHIP_PACKET_STX;
	cbDst--;

	// write
	{
		int cbTotalLen = pDst - pHip->pHipWriteBuf;

		int cbWritten = binqueue_write(pHip->pWriteQ, pHip->pHipWriteBuf, cbTotalLen);
		if (cbWritten != cbTotalLen)
		{
			fprintf(stderr, "HIP write buffer overflow\n");
			goto error;
		}
	}

	return TRUE;

error:
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierrahip_setHipHandler(sierrahip* pHip, SIERRAHIP_HIPHANDLER* lpfnHipHandler, void* pRef)
{
	// set handler
	pHip->lpfnHipHandler = lpfnHipHandler;
	pHip->pRef = pRef;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
sierrahip* sierrahip_create(void)
{
	sierrahip* pHip;

	// allocate object
	__goToErrorIfFalse(pHip = __allocObj(sierrahip));

	// initiate object
	pHip->hHipDev = -1;

	// create q
	__goToErrorIfFalse(pHip->pWriteQ = binqueue_create(SIERRAHIP_WRITE_BUF_LEN));

	__goToErrorIfFalse(pHip->pHipReadTmpBuf = (char*)__alloc(SIERRAHIP_READ_BUF_LEN));

	// allocate HIP packet
	__goToErrorIfFalse(pHip->pHipReadBuf = (char*)__alloc(SIERRAHIP_MAX_HIP_LEN));

	// allocate HIP packet
	__goToErrorIfFalse(pHip->pHipWriteBuf = (char*)__alloc(SIERRAHIP_MAX_HIP_LEN * 2));

	return pHip;

error:
	sierrahip_destroy(pHip);
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sierrahip_destroy(sierrahip* pHip)
{
	__bypassIfNull(pHip);

	sierrahip_closeHipDev(pHip);

	__free(pHip->pHipReadTmpBuf);
	__free(pHip->pHipReadBuf);
	__free(pHip->pHipWriteBuf);

	binqueue_destroy(pHip->pWriteQ);

	__free(pHip);
}

