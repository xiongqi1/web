#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#include "sslencrypt.h"

#include "base64.h"
#include "crc32.h"



///////////////////////////////////////////////////////////////////////////////
static void __fillRandom(unsigned char *achRand, size_t cbSize)
{
	struct timeval tp;
	int i = 0;
	int ran;

	gettimeofday(&tp, NULL);
	srandom(tp.tv_sec | tp.tv_usec);

	while (1)
	{
		if (!(cbSize / (4*(i + 1))))
			break;

		ran = random();
		memcpy(achRand + (i*4), (void *)&ran, 4);
		i++;
	}
}
////////////////////////////I///////////////////////////////////////////////////
static void __convertToMD128(const char* szMsg, unsigned char* achMD128)
{
	// init. MD5
	MD5_CTX lctx;
	MD5_Init(&lctx);

	// update MD5
	int cbMsgLen = strlen(szMsg);
	MD5_Update(&lctx, szMsg, cbMsgLen);

	// fini. MD5
	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5_Final(digest, &lctx);

	memcpy(achMD128, digest, sizeof(digest));
}

/////////////////////////////////////////////////////////////////////////////////
void sslencrypt_destroy(sslencrypt* pS)
{
	if (!pS)
		return;

	EVP_CIPHER_CTX_cleanup(pS->ctx);

	if (pS->pEnDecryptBuf)
		free(pS->pEnDecryptBuf);

	if (pS->ctx) {
		EVP_CIPHER_CTX_free(pS->ctx);
	}

	free(pS);
}

/////////////////////////////////////////////////////////////////////////////////
sslencrypt* sslencrypt_create(int cbMaxBuf)
{
	sslencrypt* pS;

	// allocate
	if (!(pS = malloc(sizeof(*pS))))
		goto error;

	// zero memory
	memset(pS, 0, sizeof(*pS));

	// allocate buffer
	pS->cbEnDecryptBuf = cbMaxBuf;
	pS->pEnDecryptBuf = malloc(pS->cbEnDecryptBuf);
	if (!pS->pEnDecryptBuf)
		goto error;

	// create ctx
	pS->ctx = EVP_CIPHER_CTX_new();
	if (!pS->ctx) {
		goto error;
	}

	return pS;

error:
	sslencrypt_destroy(pS);
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
void sslencrypt_setKey(sslencrypt* pS, const char* szKey)
{
	__convertToMD128(szKey, pS->achKey);
}

/////////////////////////////////////////////////////////////////////////////////
int sslencrypt_isSslEncrypted(const char* pBuf)
{
	if(strlen(pBuf)>0)
	{
		if(pBuf[0]==SSLENCRYPT_HEADER)
			return 0;
	}

	return -1;
}
static int sslencrypt_sslDecrtotCpy(EVP_CIPHER_CTX* pCtx,unsigned char* achKey, unsigned char* achVec,unsigned char* pDst,int cbDst,const unsigned char* pSrc,int cbSrc)
{
	// init.
	EVP_DecryptInit(pCtx, EVP_bf_cbc(), achKey, achVec);

	// update
	int cbUpdate;
	if (EVP_DecryptUpdate(pCtx, pDst, &cbUpdate, pSrc, cbSrc) != 1)
		goto error;

	// finial
	int cbFinal;
	if (EVP_DecryptFinal(pCtx, pDst + cbUpdate, &cbFinal) != 1)
		goto error;

	return cbUpdate + cbFinal;

error:
	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
static int sslencrypt_sslEnryptCpy(EVP_CIPHER_CTX* pCtx,unsigned char* achKey, unsigned char* achVec,unsigned char* pDst,int cbDst,const unsigned char* pSrc,int cbSrc)
{
	// init.
	EVP_EncryptInit(pCtx, EVP_bf_cbc(), achKey, achVec);

	// update
	int cbUpdate;
	if (EVP_EncryptUpdate(pCtx, pDst, &cbUpdate, pSrc, cbSrc) != 1)
		goto error;

	// final
	int cbFinal;
	if (EVP_EncryptFinal(pCtx, pDst + cbUpdate, &cbFinal) != 1)
		goto error;

	// get total length
	return cbUpdate + cbFinal;

error:
	return -1;
}
/////////////////////////////////////////////////////////////////////////////////
int sslencrypt_sslEncrypt(sslencrypt* pS, const char* szMsg, char* achBuf, int cbBuf, int* pNeeded)
{
	if (pNeeded)
		*pNeeded = 0;

	const unsigned char* pSrc=(const unsigned char*)szMsg;
	int cbMsg=strlen(szMsg);

	// get hdr and body
	decrypt_hdr* pDeHdr=(decrypt_hdr*)pS->pEnDecryptBuf;
	// get random vector
	__fillRandom(pDeHdr->achVec, sizeof(pDeHdr->achVec));

	// encrypt message
	int cbEncrypt=0;
	if(cbMsg)
	{
		unsigned char* pBody=(unsigned char*)(pDeHdr+1);
		cbEncrypt=sslencrypt_sslEnryptCpy(pS->ctx,pS->achKey,pDeHdr->achVec,pBody,pS->cbEnDecryptBuf-sizeof(*pDeHdr),pSrc,cbMsg);
		if(cbEncrypt<0)
			goto error;
	}

	// set pay load size & crc32
	pDeHdr->cbPayloadLen=(unsigned short)cbEncrypt;
	void* pCrc32Src=&pDeHdr->dwCRC32+1;
	pDeHdr->dwCRC32=crc32(pCrc32Src,cbEncrypt+sizeof(*pDeHdr)-sizeof(pDeHdr->dwCRC32),0);

	// build encrypt header - stx
	*achBuf=SSLENCRYPT_HEADER;
	char* pDst=(char*)(achBuf+1);
	int cbDst=cbBuf-1;

	// build encrypt header - vect
	int cbTotal=base64Encode(pS->pEnDecryptBuf, sizeof(*pDeHdr)+cbEncrypt, pDst, cbDst);
	if(cbTotal<0)
		goto error;

	// put NULL-termiatnion
	pDst[cbTotal]=0;

	return cbTotal+1;

error:
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int sslencrypt_sslDecrypt(sslencrypt* pS, const char* szEnMsg, char* achBuf, int cbBuf)
{
	// check header
	if(sslencrypt_isSslEncrypted(szEnMsg)<0)
		goto error;

	// build encrypt header - stx
	char* pSrc=(char*)(szEnMsg+1);
	int cbSrc=strlen(pSrc);

	// base64 decode
	int cbTLen=base64Decode(pS->pEnDecryptBuf,pS->cbEnDecryptBuf,pSrc,cbSrc);
	if(cbTLen<0)
		goto error;

	// check total length
	decrypt_hdr* pDeHdr=(decrypt_hdr*)pS->pEnDecryptBuf;
	int cbPayLoadLen=cbTLen-sizeof(decrypt_hdr);
	if(pDeHdr->cbPayloadLen!=cbPayLoadLen)
		goto error;

	// check crc32
	void* pCrc32Src=&pDeHdr->dwCRC32+1;
	if(crc32(pCrc32Src,cbPayLoadLen+sizeof(*pDeHdr)-sizeof(pDeHdr->dwCRC32),0)!=pDeHdr->dwCRC32)
		goto error;

	// decrypt
	int cbTotal=0;
	unsigned char* pDst=(unsigned char*)achBuf;
	if(cbPayLoadLen)
	{
		unsigned char* pBody=(unsigned char*)(pDeHdr+1);
		cbTotal=sslencrypt_sslDecrtotCpy(pS->ctx,pS->achKey,pDeHdr->achVec,pDst,cbBuf,pBody,cbPayLoadLen);
		if(cbTotal<0)
			goto error;
	}

	pDst[cbTotal]=0;

	return cbTotal+1;

error:
	return -1;
}
