/*

md5evp.c

gSOAP HTTP Content-MD5 digest EVP handler for httpmd5 plugin

gSOAP XML Web services tools
Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc., All Rights Reserved.
This part of the software is released under one of the following licenses:
GPL, the gSOAP public license, or Genivia's license for commercial use.
--------------------------------------------------------------------------------
gSOAP public license.

The contents of this file are subject to the gSOAP Public License Version 1.3
(the "License"); you may not use this file except in compliance with the
License. You may obtain a copy of the License at
http://www.cs.fsu.edu/~engelen/soaplicense.html
Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the License.

The Initial Developer of the Original Code is Robert A. van Engelen.
Copyright (C) 2000-2005, Robert van Engelen, Genivia, Inc., All Rights Reserved.
--------------------------------------------------------------------------------
GPL license.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

Author contact information:
engelen@genivia.com / engelen@acm.org
--------------------------------------------------------------------------------
A commercial use license is available from Genivia, Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

#include "md5evp.h"
#include "debug.h"

int md5_handler(struct soap *soap, void **context, enum md5_action action, char *buf, size_t len)
{
	EVP_MD_CTX *ctx;
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int size;

	DEBUG_OUTPUT (
		dbglog (SVR_DEBUG, DBG_AUTH,
				"md5_handler: soap=%p context=%p action=%d len=%d\n",
				soap, *context, action, len);
	)

  switch (action)
  { case MD5_INIT:
#ifdef WITH_OPENSSL
      OpenSSL_add_all_digests();
#endif
      if (!*context)
      { *context = (void*)SOAP_MALLOC(soap, sizeof(EVP_MD_CTX));
        EVP_MD_CTX_init((EVP_MD_CTX*)*context);
      }
      ctx = (EVP_MD_CTX*)*context;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- MD5 Init %p\n", ctx));

      DEBUG_OUTPUT (
    		  dbglog (SVR_DEBUG, DBG_AUTH,
    				  "md5_handler: MD5_INIT->EVP_DigestInit->pre ctx=%p ctx->md_data=%p\n",
    				  ctx, ctx->md_data);
      )

#ifndef DIMARK
      /* 080715 0000018 Dimark                                                     */
      /* The malloc()'d data buffer pointed to by ctx->md_data is used internally  */
      /* by the OpenSSL system.  It really doesn't get exposed even to here.       */
      /* EVP_MD_CTX_init() simply does a memset(ctx, '\0', sizeof *ctx) internally */
      /* and is called also at the top of EVP_DigestInit();  What we've observed   */
      /* is that ctx->md_data is still allocated when EVP_DigestInit() is called   */
      /* when operating with basic or digest authentication.  This has the effect  */
      /* of loosing the pointer to the previously malloc()'d buffer, never freeing */
      /* it up and therefore creating a memory leak.                               */
      /* ctx-md_data is malloc()'s internally in EVP_DigestInit();                 */
      /* The memory for ctx->md_data is free()'d up in the call the MD5_DELETE     */
      /* This fix is official until at some point in the future, other code in the */
      /* Dimark client is re-organized making this fix redundant                   */

      if (ctx->md_data) {
    	  DEBUG_OUTPUT (
        		  dbglog (SVR_DEBUG, DBG_AUTH,
        				  "md5_handler: MD5_INIT->EVP_DigestInit->pre freeing ctx->md_data=%p\n",
        				  ctx->md_data);
    	  )

    	  SOAP_FREE(soap, ctx->md_data);

#if defined(PLATFORM_PLATYPUS)
//    	  free(ctx->md_data);
#endif
    	  ctx->md_data = NULL;
      }
#endif

      EVP_DigestInit(ctx, EVP_md5());

	  DEBUG_OUTPUT (
    		  dbglog (SVR_DEBUG, DBG_AUTH,
    				  "md5_handler: MD5_INIT->EVP_DigestInit->post ctx=%p ctx->md_data=%p\n",
    				  ctx, ctx->md_data);
	  )

      break;
    case MD5_UPDATE:
      ctx = (EVP_MD_CTX*)*context;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- MD5 Update %p --\n", ctx));
      DBGMSG(TEST, buf, len);
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "\n--"));
      EVP_DigestUpdate(ctx, (void*)buf, (unsigned int)len);
      break;
    case MD5_FINAL:
      ctx = (EVP_MD_CTX*)*context;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- MD5 Final %p --\n", ctx));
      EVP_DigestFinal(ctx, (void*)hash, &size);
      memcpy(buf, hash, 16);
      break;
    case MD5_DELETE:
      ctx = (EVP_MD_CTX*)*context;
      DBGLOG(TEST, SOAP_MESSAGE(fdebug, "-- MD5 Delete %p --\n", ctx));
      if (ctx)
      { 
        EVP_MD_CTX_cleanup(ctx);
        SOAP_FREE(soap, ctx);
        /* 080618 mark added for testing */
        ctx = NULL;
        fflush(stdout);
//        fsync(stdout);
        /* mark to here */
      }
      *context = NULL;
      fflush(stdout);
//      fsync(stdout);
  }
  return SOAP_OK;
}
