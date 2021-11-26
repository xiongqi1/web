/*****************************************************************************
*
*    Copyright (C) 2005 Paul Dwerryhouse
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; version 2 of the License.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*****************************************************************************/

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "stun_util.h"

char **serialise(char **b, int *len, const char *fmt, ...)
{

    const char *p;
    char *buf, *t;
    va_list argp;

    (*len) = 0;

    va_start(argp, fmt);
    for (p = fmt; *p; p++) {
	switch (*p) {
	case 'c':
	    (*len)++;
	    (void) va_arg(argp, int);
	    break;
	case 's':
	    (*len) += 2;
	    (void) va_arg(argp, int);
	    break;
	case 'l':
	    (*len) += 4;
	    (void) va_arg(argp, int);
	    break;
	case 'L':
	    (*len) += 8;
	    (void) va_arg(argp, int);
	    break;
	case 'b':
	    (void) va_arg(argp, char *);
	    (*len) += va_arg(argp, int);
	    break;
	}

    }
    va_end(argp);

    buf = (char *) malloc(sizeof(char) * (*len));
    if (!buf)
	return 0;

    t = buf;

    va_start(argp, fmt);

    for (p = fmt; *p; p++) {
	char c;
	short s;
	long l;
#ifdef HAVE_LONGLONG
	long long ll;
#endif
	char *b;
	int tmplen;

	switch (*p) {
	case 'c':
	    c = (char) va_arg(argp, int);
	    memcpy(t, &c, sizeof(unsigned char));
	    t += sizeof(unsigned char);
	    break;
	case 's':
	    s = htons((short) va_arg(argp, int));
	    memcpy(t, &s, sizeof(unsigned short));
	    t += sizeof(unsigned short);
	    break;
	case 'l':
	    l = htonl(va_arg(argp, unsigned long));
	    memcpy(t, &l, sizeof(unsigned long));
	    t += sizeof(unsigned long);
	    break;
#ifdef HAVE_LONGLONG
	case 'L':
	    ll = va_arg(argp, unsigned long long);
	    memcpy(t, &ll, sizeof(unsigned long long));
	    t += sizeof(unsigned long long);
	    break;
#endif
	case 'b':
	    b = va_arg(argp, char *);
	    tmplen = va_arg(argp, int);
	    memcpy(t, b, tmplen);
	    t += tmplen;
	    break;
	}
    }

    va_end(argp);

    *b = buf;

    return b;
}

char *memcat(char **dest, int len, const char *src, size_t n)
{
    char *t;

    *dest = realloc(*dest, sizeof(char) * (len + n));
    if (!*dest)
	return 0;

    t = *dest + len;
    while (n--) {
	*(t++) = *(src++);
    }

    return *dest;
}

char *addr_ntoa(unsigned long addr)
{
    struct in_addr in;

    in.s_addr = ntohl(addr);
    return (inet_ntoa(in));
}

#endif /* WITH_STUN_CLIENT */
