#include <string.h>
#include <ctype.h>

///////////////////////////////////////////////////////////////////////////////

const signed char HEX2DEC[256] =
{
	/*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
	/* 0 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 1 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 2 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9, -1, -1, -1, -1, -1, -1,

	/* 4 */ -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 5 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 6 */ -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 7 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

	/* 8 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* 9 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* A */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* B */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

	/* C */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* D */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* E */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	/* F */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

///////////////////////////////////////////////////////////////////////////////
int uriDecode(const char* szUri, char* achBuf, int len)
{
	// Note from RFC1630:  "Sequences which start with a percent sign
	// but are not followed by two hexadecimal characters (0-9, A-F) are reserved
	// for future extension"

	const unsigned char * pSrc = (const unsigned char *)szUri;
	const int SRC_LEN = strlen(szUri);
	const unsigned char * const SRC_END = pSrc + SRC_LEN;
	const unsigned char * SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'

	char * const pStart = achBuf;
	char * pEnd = pStart;
	
	int count=0;

	while (pSrc < SRC_LAST_DEC)
	{
		if(count>=len)
			break;
		if (*pSrc == '%')
		{
			signed char dec1, dec2;
			if (-1 != (dec1 = HEX2DEC[*(pSrc + 1)]) && -1 != (dec2 = HEX2DEC[*(pSrc + 2)]))
			{
				*pEnd++ = (dec1 << 4) + dec2;
				count++;
				pSrc += 3;
				continue;
			}
		}

		*pEnd++ = *pSrc++;
		count++;
	}

	// the last 2- chars
	while ((pSrc < SRC_END) && (count<len)) {
		*pEnd++ = *pSrc++;
		count++;
	}

	if(count>=len)
		pEnd--;
		
	*pEnd++ = 0;
	
	return pEnd -pStart;
}

///////////////////////////////////////////////////////////////////////////////

static const char DEC2HEX[] = "0123456789ABCDEF";
///////////////////////////////////////////////////////////////////////////////
int uriEncode(const char* szStr, char* achBuf, int len)
{
	const unsigned char * pSrc = (const unsigned char *)szStr;
	unsigned char * pDst = (unsigned char *) achBuf;
	unsigned char val;
	int count;

	for (count = 0; (count < len-1) && *pSrc; ++pSrc)
	{
		val = *pSrc;
		if (isalnum(val)) {
			*pDst++ = val;
			count++;
		}
		else
		{
			if (count <= len-4) {
				// escape this char
				*pDst++ = '%';
				*pDst++ = DEC2HEX[val >> 4];
				*pDst++ = DEC2HEX[val & 0x0F];
				count += 3;
			}
			else {
				break;
			}
		}
	}

	*pDst++ = 0;

	return pSrc - (const unsigned char *)szStr;
}

///////////////////////////////////////////////////////////////////////////////
