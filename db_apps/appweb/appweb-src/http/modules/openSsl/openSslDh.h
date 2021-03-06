///
///	@file 	openSslDh.h
/// @brief 	OpenSSL DH get routines
//	@copy	default
//	
//	Copyright (c) Mbedthis Software LLC, 2003-2007. All Rights Reserved.
//	
//	This software is distributed under commercial and open source licenses.
//	You may use the GPL open source license described below or you may acquire 
//	a commercial license from Mbedthis Software. You agree to be fully bound 
//	by the terms of either license. Consult the LICENSE.TXT distributed with 
//	this software for full details.
//	
//	This software is open source; you can redistribute it and/or modify it 
//	under the terms of the GNU General Public License as published by the 
//	Free Software Foundation; either version 2 of the License, or (at your 
//	option) any later version. See the GNU General Public License for more 
//	details at: http://www.mbedthis.com/downloads/gplLicense.html
//	
//	This program is distributed WITHOUT ANY WARRANTY; without even the 
//	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
//	
//	This GPL license does NOT permit incorporating this software into 
//	proprietary programs. If you are unable to comply with the GPL, you must
//	acquire a commercial license to use this software. Commercial licenses 
//	for this software and support services are available from Mbedthis 
//	Software at http://www.mbedthis.com 
//	
//	@end
////////////////////////////////////////////////////////////////////////////////

#ifndef HEADER_DH_H
#include <openssl/opensslv.h>
#include <openssl/dh.h>
#endif

DH *get_dh512()
	{
	static unsigned char dh512_p[]={
		0x8E,0xFD,0xBE,0xD3,0x92,0x1D,0x0C,0x0A,0x58,0xBF,0xFF,0xE4,
		0x51,0x54,0x36,0x39,0x13,0xEA,0xD8,0xD2,0x70,0xBB,0xE3,0x8C,
		0x86,0xA6,0x31,0xA1,0x04,0x2A,0x09,0xE4,0xD0,0x33,0x88,0x5F,
		0xEF,0xB1,0x70,0xEA,0x42,0xB6,0x0E,0x58,0x60,0xD5,0xC1,0x0C,
		0xD1,0x12,0x16,0x99,0xBC,0x7E,0x55,0x7C,0xE4,0xC1,0x5D,0x15,
		0xF6,0x45,0xBC,0x73,
		};
	static unsigned char dh512_g[]={
		0x02,
		};
	DH *dh;

	if ((dh=DH_new()) == NULL) return(NULL);
	#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    // OpenSSL v1.1.0 has public API changes
    // p, g and length of the Diffie-Hellman param can't be set directly anymore,
    // instead DH_set0_pqg and DH_set_length are used
    BIGNUM* dh_p = BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
    BIGNUM* dh_g = BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
    if ((dh_p == NULL) || (dh_g == NULL) || !DH_set0_pqg(dh, dh_p, NULL, dh_g))
	#else
    dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
	dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
	#endif
		{ DH_free(dh); return(NULL); }
	return(dh);
	}
#ifndef HEADER_DH_H
#include <openssl/dh.h>
#endif
DH *get_dh1024()
	{
	static unsigned char dh1024_p[]={
		0xCD,0x02,0x2C,0x11,0x43,0xCD,0xAD,0xF5,0x54,0x5F,0xED,0xB1,
		0x28,0x56,0xDF,0x99,0xFA,0x80,0x2C,0x70,0xB5,0xC8,0xA8,0x12,
		0xC3,0xCD,0x38,0x0D,0x3B,0xE1,0xE3,0xA3,0xE4,0xE9,0xCB,0x58,
		0x78,0x7E,0xA6,0x80,0x7E,0xFC,0xC9,0x93,0x3A,0x86,0x1C,0x8E,
		0x0B,0xA2,0x1C,0xD0,0x09,0x99,0x29,0x9B,0xC1,0x53,0xB8,0xF3,
		0x98,0xA7,0xD8,0x46,0xBE,0x5B,0xB9,0x64,0x31,0xCF,0x02,0x63,
		0x0F,0x5D,0xF2,0xBE,0xEF,0xF6,0x55,0x8B,0xFB,0xF0,0xB8,0xF7,
		0xA5,0x2E,0xD2,0x6F,0x58,0x1E,0x46,0x3F,0x74,0x3C,0x02,0x41,
		0x2F,0x65,0x53,0x7F,0x1C,0x7B,0x8A,0x72,0x22,0x1D,0x2B,0xE9,
		0xA3,0x0F,0x50,0xC3,0x13,0x12,0x6C,0xD2,0x17,0xA9,0xA5,0x82,
		0xFC,0x91,0xE3,0x3E,0x28,0x8A,0x97,0x73,
		};
	static unsigned char dh1024_g[]={
		0x02,
		};
	DH *dh;

	if ((dh=DH_new()) == NULL) return(NULL);
	#if OPENSSL_VERSION_NUMBER >= 0x10100003L
    // OpenSSL v1.1.0 has public API changes
    // p, g and length of the Diffie-Hellman param can't be set directly anymore,
    // instead DH_set0_pqg and DH_set_length are used
    BIGNUM* dh_p = BN_bin2bn(dh1024_p,sizeof(dh1024_p),NULL);
    BIGNUM* dh_g = BN_bin2bn(dh1024_p,sizeof(dh1024_p),NULL);
    if ((dh_p == NULL) || (dh_g == NULL) || !DH_set0_pqg(dh, dh_p, NULL, dh_g))
	#else
	dh->p=BN_bin2bn(dh1024_p,sizeof(dh1024_p),NULL);
	dh->g=BN_bin2bn(dh1024_g,sizeof(dh1024_g),NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
	#endif
		{ DH_free(dh); return(NULL); }
	return(dh);
	}

//
// Local variables:
// tab-width: 4
// c-basic-offset: 4
// End:
// vim: sw=4 ts=4 
//
