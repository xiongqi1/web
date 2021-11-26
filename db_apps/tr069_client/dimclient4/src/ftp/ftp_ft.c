/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

char copyright[] =
  "@(#) Copyright (c) 1985, 1989 Regents of the University of California.\n"
  "All rights reserved.\n";

/*
 * from: @(#)main.c	5.18 (Berkeley) 3/1/91
 */


/*
 * FTP User Program -- Command Interface.
 */
#include "globals.h"

#ifdef HAVE_FILE

#include "cmds.h"
#include "ftp_var.h"

extern int data;
extern int NCMDS;

void intr(int);
void lostpeer(int);

int
ftp_login(char *hostname, const char *username, const char *password )
{
	ftp_port = htons(21);
	passivemode = 0;

	cpend = 0;	//* no pending replies 
	proxy = 0;	//* proxy not active 
	sendport = -1;	//* not using ports 
	/*
	 * Connect to the remote server
	 */
	connected = doLogin( hostname, username, password);
	return ( connected == 1 ? OK : ERR_DOWNLOAD_FAILURE );
}

int 
ftp_get( char *remoteFile, char *localFile, long *size )
{
	*size = 0;
	return dimget( remoteFile, localFile, size );
}

int 
ftp_put( char *remoteFile, char *localFile, long *size )
{
	*size = 0;
	return dimput( remoteFile, localFile, size );
}

void 
ftp_disconnect (void)
{
	disconnect();
}

#endif /* HAVE_FILE */
