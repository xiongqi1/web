#include "smsdef.h"

#include <ctype.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int cnsConvPhoneNumber(const char* pDial, unsigned char* pDst, int cbDst, BOOL* pInternational)
{
	unsigned char* pOut = pDst;
	BOOL fAccepted = FALSE;
	int nOut;
	char chDial;

	// reset international
	*pInternational = FALSE;

	while (__isTrue(chDial = *pDial++))
	{
		// if digit
		if (isdigit(chDial))
		{
			fAccepted = TRUE;
			nOut = chDial - '0';
		}
		// if asterisk
		else if (chDial == '*')
		{
			fAccepted = TRUE;
			nOut = 0x0a;
		}
		// if comma
		else if (chDial == ',')
		{
			fAccepted = TRUE;
			nOut = 0x0c;
		}
		// if hash
		else if (chDial == '#')
		{
			fAccepted = TRUE;
			nOut = 0x0b;
		}
		// if question mark
		else if (chDial == '?')
		{
			fAccepted = TRUE;
			nOut = 0x0d;
		}
		// if plus
		else if (!fAccepted && chDial == '+')
		{
			if (pInternational)
				*pInternational = TRUE;
			nOut = -1;
		}
		else
		{
			nOut = -1;
		}

		// out
		if (nOut >= 0)
		{
			if (__isFalse(pOut < pDst + cbDst))
				return -1;

			*pOut++ = (unsigned char)nOut;
		}
	}

	return (int)(pOut -pDst);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cnsConvSmscAddressField(const char* pAddress, int cbAddr, unsigned char* pDst)
{
	int i;
	for (i = 0; i < cbAddr; i++, pDst++, pAddress++) {
		*pDst = *pAddress + '0';
	}
}

void printMsgBody(char* msg, int len)
{
#ifdef SMS_DEBUG

	char* buf = msg;
	char buf2[BSIZE_256] = {0x0,};
	int i, j = len/16, k = len % 16;
	syslog(LOG_ERR, "---- sms msg body ---");
	for (i = 0; i < j; i++)	{
		syslog(LOG_ERR, "%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
	}
	j = i;
	for (i = 0; i < k; i++)
		sprintf(buf2, "%s%02x, ", buf2, buf[j*16+i]);
	syslog(LOG_ERR, "%s", buf2);
#endif
}

void printMsgBodyInt(int* msg, int len)
{
#ifdef SMS_DEBUG
    int* buf = msg;
	char buf2[BSIZE_256]={0x0,};
    int i, j = len/16, k = len % 16;
    syslog(LOG_ERR, "---- sms msg body ---");
    for (i = 0; i < j; i++) {
        syslog(LOG_ERR, "%04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x, %04x",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
    }
    j = i;
    for (i = 0; i < k; i++)
        sprintf(buf2, "%s%04x, ", buf2, buf[j*16+i]);
    syslog(LOG_ERR, "%s", buf2);
#endif
}

