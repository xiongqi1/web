#ifndef __BASE64_H__
#define __BASE64_H__


int base64GetEncodeLen(int cbDecode);
int base64Encode(unsigned char* pSrc, int cbSrc, char* pDst, int cbDst);

int base64GetEncodeLen(int cbDecode);
int base64Decode(unsigned char* pDst, int cbDst,const char* pSrc,int cbSrc);

#endif

