#ifndef __SSLENCRYPT_H__
#define __SSLENCRYPT_H__

#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/md4.h>

#include "base.h"

#define SSLENCRYPT_HEADER			'$'

typedef struct
{
	unsigned int dwCRC32;					// crc32 calculation after itself

	unsigned char achVec[8];	
	unsigned short cbPayloadLen;
} __packedStruct decrypt_hdr;

typedef struct
{
	unsigned char achKey[EVP_MAX_KEY_LENGTH];

	EVP_CIPHER_CTX* ctx;

	int cbEnDecryptBuf;
	unsigned char* pEnDecryptBuf;

} sslencrypt;



sslencrypt* sslencrypt_create(int cbMaxBuf);
void sslencrypt_destroy(sslencrypt* pS);
int sslencrypt_sslEncrypt(sslencrypt* pS, const char* szMsg, char* achBuf, int cbBuf, int* pNeeded);
int sslencrypt_sslDecrypt(sslencrypt* pS, const char* szEnMsg, char* achBuf, int cbBuf);
void sslencrypt_setKey(sslencrypt* pS, const char* szKey);
int sslencrypt_isSslEncrypted(const char* pBuf);

#endif
