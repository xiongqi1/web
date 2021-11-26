#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"
#include "base.h"

///////////////////////////////////////////////////////////////////////////////
static char encode(unsigned char u)
{

	if (u < 26)  return 'A' + u;
	if (u < 52)  return 'a' + (u - 26);
	if (u < 62)  return '0' + (u - 52);
	if (u == 62) return '+';

	return '/';
}

///////////////////////////////////////////////////////////////////////////////
int base64GetDecodeLne(int cbEncode)
{
	return (cbEncode*6+7)/8;
}
///////////////////////////////////////////////////////////////////////////////
int base64GetEncodeLen(int cbDecode)
{
	return (cbDecode*8 + 5) / 6;
}

///////////////////////////////////////////////////////////////////////////////
int base64Encode(unsigned char* pSrc, int cbSrc, char* pDst, int cbDst)
{
	// bypass if NULL
	if (!pSrc)
		return 0;

	// error if less space
	int cbEncode = base64GetEncodeLen(cbSrc);
	if (cbDst < cbEncode)
		return -1;

	int i;
	char *p;

	p = pDst;

	for (i = 0; i < cbSrc; i += 3)
	{
		unsigned char b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, b7 = 0;

		b1 = pSrc[i];

		if (i + 1 < cbSrc)
			b2 = pSrc[i+1];

		if (i + 2 < cbSrc)
			b3 = pSrc[i+2];

		b4 = b1 >> 2;
		b5 = ((b1 & 0x3) << 4) | (b2 >> 4);
		b6 = ((b2 & 0xf) << 2) | (b3 >> 6);
		b7 = b3 & 0x3f;

		*p++ = encode(b4);
		*p++ = encode(b5);

		if (i + 1 < cbSrc)
		{
			*p++ = encode(b6);
		}
		else
		{
			*p++ = '=';
		}

		if (i + 2 < cbSrc)
		{
			*p++ = encode(b7);
		}
		else
		{
			*p++ = '=';
		}

	}

	return p-pDst;

}

///////////////////////////////////////////////////////////////////////////////
static int is_base64(char c)
{

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
	        (c >= '0' && c <= '9') || (c == '+')             ||
	        (c == '/')             || (c == '='))
	{

		return TRUE;

	}

	return FALSE;

}

///////////////////////////////////////////////////////////////////////////////
static unsigned char decode(char c)
{

	if (c >= 'A' && c <= 'Z') return(c - 'A');
	if (c >= 'a' && c <= 'z') return(c - 'a' + 26);
	if (c >= '0' && c <= '9') return(c - '0' + 52);
	if (c == '+')             return 62;

	return 63;

}

///////////////////////////////////////////////////////////////////////////////
int base64Decode(unsigned char* pDst, int cbDst,const char* pSrc,int cbSrc)
{
	if(!pSrc)
		return 0;

	//int cbDecode=base64GetDecodeLne(cbSrc);
	//if(cbDst<cbDecode)
	//	return -1;

		unsigned char *p = pDst;
		int k, l = strlen(pSrc) + 1;

		/* Ignore non base64 chars as per the POSIX standard */
		for (k = 0, l = 0; pSrc[k]; k++)
		{

			if (is_base64(pSrc[k]))
				pDst[l++] = pSrc[k];
		}

		for (k = 0; k < l; k += 4)
		{
			char c1 = 'A', c2 = 'A', c3 = 'A', c4 = 'A';
			unsigned char b1 = 0, b2 = 0, b3 = 0, b4 = 0;

			c1 = pDst[k];

			if (k + 1 < l)
				c2 = pDst[k+1];

			if (k + 2 < l)
				c3 = pDst[k+2];

			if (k + 3 < l)
				c4 = pDst[k+3];

			b1 = decode(c1);
			b2 = decode(c2);
			b3 = decode(c3);
			b4 = decode(c4);

			*p++ = ((b1 << 2) | (b2 >> 4));

			if (c3 != '=')
				*p++ = (((b2 & 0xf) << 4) | (b3 >> 2));

			if (c4 != '=')
				*p++ = (((b3 & 0x3) << 6) | b4);
		}

		return(p-pDst);

	
	return FALSE;

}



