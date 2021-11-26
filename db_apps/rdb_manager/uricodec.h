#ifndef __URICODEC_H__
#define __URICODEC_H__

int uriEncode(const char* szStr, char* achBuf, int len);
int uriDecode(const char* szUri, char* achBuf, int len);

#endif

