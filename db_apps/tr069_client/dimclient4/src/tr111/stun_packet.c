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

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "stun_packet.h"
#include "stun_util.h"

static 	u_int16_t con_req_bin_attr = STUN_CONNECTION_REQUEST_BIN_ATTR;
static 	u_int16_t bin_cha_attr = STUN_BINDING_CHANGE_ATTR;

void create_random_tid(stun_tid_t * tid)
{
    int i;

    for (i = 0; i < sizeof(tid->data); i++) {
	tid->data[i] = rand() % 256;
    }
}

struct packet *packet_create(u_int16_t type, stun_tid_t tid)
{

    struct packet *p;

    p = (struct packet *) malloc(sizeof(struct packet));
    if (!p)
	return 0;

    p->type = type;
    p->len = 0;

    memcpy(p->tid.data, tid.data, sizeof(tid.data));

    p->username = p->password = 0;
    p->mapped_address = 0;
    p->response_address = 0;
    p->change_request = 0;
    p->source_address = 0;
    p->changed_address = 0;
    p->username = 0;
    p->password = 0;
    p->message_integrity = 0;
    p->error_code = 0;
    p->unknown_attributes = 0;
    p->reflected_from = 0;

    p->binding_change_attr = 0;
    p->connection_request_binding_attr = 0;

    return p;
}

struct address *add_address(u_int32_t address, u_int16_t port)
{
    struct address *addr;

    addr = (struct address *) malloc(sizeof(struct address));
    if (!addr)
	return 0;

    addr->family = 0x01;
    addr->port = port;
    addr->address = address;


    return addr;
}

struct stun_string *add_string(char *s)
{
    struct stun_string *str;
    int len;

    str = (struct stun_string *) malloc(sizeof(struct stun_string));
    if (!str)
	return 0;

    str->data = 0;
    str->len = 0;

    len = strlen(s);
    len = (len + 3) - ((len + 3) % 4);

    str->data = (char *) calloc(len, sizeof(char));
    if (!str->data) {
	free(str);
	return 0;
    }

    str->len = len;

    strcpy(str->data, s);

    return str;
}

int add_mapped_address(struct packet *p, u_int32_t address, u_int16_t port)
{

    struct address *addr = add_address(address, port);

    if (!addr)
	return 0;

    p->mapped_address = addr;
    p->len += 12;

    return 1;
}

int
add_response_address(struct packet *p, u_int32_t address, u_int16_t port)
{

    struct address *addr = add_address(address, port);

    if (!addr)
	return 0;

    p->response_address = addr;
    p->len += 12;

    return 1;
}

int
add_changed_address(struct packet *p, u_int32_t address, u_int16_t port)
{

    struct address *addr = add_address(address, port);

    if (!addr)
	return 0;

    p->changed_address = addr;
    p->len += 12;

    return 1;
}

int add_source_address(struct packet *p, u_int32_t address, u_int16_t port)
{

    struct address *addr = add_address(address, port);

    if (!addr)
	return 0;

    p->source_address = addr;
    p->len += 12;

    return 1;
}

int add_reflected_from(struct packet *p, u_int32_t address, u_int16_t port)
{

    struct address *addr = add_address(address, port);

    if (!addr)
	return 0;

    p->reflected_from = addr;
    p->len += 12;

    return 1;
}

int add_username(struct packet *p, char *username)
{
    p->username = add_string(username);
    if (!p->username)
	return 0;

    p->len += p->username->len + 4;

    return 1;
}

int add_password(struct packet *p, char *password)
{
    p->password = add_string(password);
    if (!p->password)
	return 0;

    p->len += p->password->len + 4;

    return 1;
}

int add_change_request(struct packet *p, unsigned char a, unsigned char b)
{
    p->change_request = (struct request *) malloc(sizeof(struct request));
    if (!p->change_request)
	return 0;

    p->change_request->a = a;
    p->change_request->b = b;

    p->len += 8;
    return 1;
}


// San 04 june 2011:
int add_binding_change_attr(struct packet *p)
{
    //p->binding_change_attr = (u_int16_t *) malloc(sizeof(u_int16_t));
    //if (!p->binding_change_attr)
	//return 0;



    p->binding_change_attr = &bin_cha_attr;

    p->len += 8;
    return 1;
}

// San 04 june 2011:
int add_connection_request_binding_attr(struct packet *p)
{
    //p->connection_request_binding_attr = (u_int16_t *) malloc(sizeof(u_int16_t));
    //if (!p->connection_request_binding_attr)
	//return 0;

    //*(p->connection_request_binding_attr) = STUN_AT_CONNECTION_REQUEST_BIN_ATTR;
    p->connection_request_binding_attr = &con_req_bin_attr;

    p->len += 8;
    return 1;
}




struct packet_s *serialise_packet(struct packet *p)
{
    struct packet_s *s;
    int len;
    char *b;

    s = (struct packet_s *) malloc(sizeof(struct packet_s));
    if (!s)
	return 0;

    serialise(&(s->data), &(s->len), "ssb",
	      p->type, p->len, p->tid.data, sizeof(p->tid.data));

    if (p->mapped_address) {
	serialise(&b, &len, "ssccsl", STUN_AT_MAPADD, 8, 0,
		  p->mapped_address->family,
		  p->mapped_address->port, p->mapped_address->address);
	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->response_address) {
	serialise(&b, &len, "ssccsl", STUN_AT_RESADD, 8, 0,
		  p->response_address->family,
		  p->response_address->port, p->response_address->address);
	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->changed_address) {
	serialise(&b, &len, "ssccsl", STUN_AT_CHGADD, 8, 0,
		  p->changed_address->family,
		  p->changed_address->port, p->changed_address->address);
	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->source_address) {
	serialise(&b, &len, "ssccsl", STUN_AT_SRCADD, 8, 0,
		  p->source_address->family,
		  p->source_address->port, p->source_address->address);
	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->reflected_from) {
	serialise(&b, &len, "ssccsl", STUN_AT_REFFRM, 8, 0,
		  p->reflected_from->family,
		  p->reflected_from->port, p->reflected_from->address);
	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->username) {
	serialise(&b, &len, "ssb", STUN_AT_USRNAM, p->username->len,
		  p->username->data, p->username->len);

	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->change_request) {
	unsigned char change[4] = "\0\0\0\0";

	change[3] =
	    p->change_request->a * 0x04 | p->change_request->b * 0x02;

	serialise(&b, &len, "ssb", STUN_AT_CHGREQ, 4, change, 4);

	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->unknown_attributes) {
	int n = p->unknown_attributes->n;
	int l = (n % 2 == 0) ? n : n + 1;

	serialise(&b, &len, "ssb", STUN_AT_UNKATT, l * 2,
		  p->unknown_attributes->attr, l * 2);

	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    if (p->error_code) {
	serialise(&b, &len, "sscccc", STUN_AT_ERRCOD, 4,
		  0, 0, p->error_code->errclass, p->error_code->errnum);

	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }


    // San 04 june 2011:
    if (p->connection_request_binding_attr) {
	unsigned char conn_req_bind_attrValue[20] = {0x64,0x73,0x6C,0x66,0x6F,0x72,0x75,0x6D,0x2E,0x6F,0x72,0x67,0x2F,0x54,0x52,0x2D,0x31,0x31,0x31,0x20};
	//"dslforum.org/TR-111 ";

	serialise(&b, &len, "ssb", STUN_CONNECTION_REQUEST_BIN_ATTR, 20, conn_req_bind_attrValue, 20);

	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    // San 04 june 2011:
    if (p->binding_change_attr) {
	serialise(&b, &len, "ss", STUN_BINDING_CHANGE_ATTR, 0, 0);
	memcat(&(s->data), s->len, b, len);
	s->len += len;
	free(b);
    }

    return s;
}

struct address *address_parse(unsigned char *buf)
{
    struct address *addr;

    addr = (struct address *) malloc(sizeof(struct address));
    addr->family = *(buf + 1);
    addr->port = (*(buf + 2) << 8) + *(buf + 3);
    addr->address = (*(buf + 4) << 24) +
	(*(buf + 5) << 16) + (*(buf + 6) << 8) + *(buf + 7);

    return addr;
}

struct packet *packet_parse(unsigned char *buf, int n)
{
    struct packet *p;
    unsigned char *b = buf, *c;

    short type, len;
    stun_tid_t tid;

    if (n < 20)
	return 0;		/* Packet too short */

    memcpy(&type, b, 2);
    type = ntohs(type);

    memcpy(&len, b + 2, 2);
    len = ntohs(len);

    if (len != (n - 20))
	return 0;		/* Packet too short */

    memcpy(tid.data, b + 4, sizeof(tid.data));

    b += 20;

    c = b;

    p = packet_create(type, tid);
    p->type = type;
    p->len = len;

    while ((c + len - b) >= 4 ) {
	struct address *addr;
	unsigned short l;
	u_int16_t t;

	memcpy(&t, b, 2);
	t = ntohs(t);

	memcpy(&l, b + 2, 2);
	l = ntohs(l);

	if (((b - c) + l + 4) > n) {
	    destroy_packet(p);
	    return 0;
	}

	switch (t) {
	case STUN_AT_MAPADD:
	    addr = address_parse(b + 4);
	    p->mapped_address = addr;
	    p->len += 12;
	    break;
	case STUN_AT_RESADD:
	    addr = address_parse(b + 4);
	    p->response_address = addr;
	    p->len += 12;
	    break;
	case STUN_AT_CHGADD:
	    addr = address_parse(b + 4);
	    p->changed_address = addr;
	    p->len += 12;
	    break;
	case STUN_AT_SRCADD:
	    addr = address_parse(b + 4);
	    p->source_address = addr;
	    p->len += 12;
	    break;
	case STUN_AT_REFFRM:
	    addr = address_parse(b + 4);
	    p->reflected_from = addr;
	    p->len += 12;
	    break;
	case STUN_AT_CHGREQ:
	case STUN_AT_USRNAM:
	case STUN_AT_PASSWD:
	case STUN_AT_MSGINT:
	case STUN_AT_ERRCOD:
	case STUN_AT_UNKATT:
	default:
	    if (t <= 0x7fff)
		add_unknown_attribute(p, t);
	    break;
	}

	b += l + 4;
    }

    return p;
}

void destroy_packet(struct packet *p)
{
    if (p->mapped_address)
	free(p->mapped_address);
    if (p->response_address)
	free(p->response_address);
    if (p->change_request)
	free(p->change_request);
    if (p->source_address)
	free(p->source_address);
    if (p->changed_address)
	free(p->changed_address);
    if (p->reflected_from)
	free(p->reflected_from);
    if (p->username) {
	if (p->username->data)
	    free(p->username->data);
	free(p->username);
    }
    if (p->password) {
	if (p->password->data)
	    free(p->password->data);
	free(p->password);
    }
    if (p->message_integrity)
	free(p->message_integrity);
    if (p->error_code)
	free(p->error_code);
    if (p->unknown_attributes)
	free(p->unknown_attributes);
    free(p);
}

void destroy_packet_s(struct packet_s *ps)
{
    if (ps->data)
	free(ps->data);
    free(ps);
}

int add_unknown_attribute(struct packet *p, u_int16_t attr)
{

    unsigned int n;

    if (!p->unknown_attributes) {
	p->unknown_attributes =
	    (struct attr_list *) malloc(sizeof(struct attr_list));
	if (!p->unknown_attributes)
	    return 0;

	p->unknown_attributes->n = 0;
	p->unknown_attributes->attr = 0;
	p->len += 4;
    }

    n = p->unknown_attributes->n;

    if (n % 2 == 0) {
	p->unknown_attributes->attr =
	    (u_int16_t *) realloc
	    (p->unknown_attributes->attr, sizeof(u_int16_t) * (n + 2));

	if (!p->unknown_attributes->attr)
	    return 0;

	p->unknown_attributes->attr[n] =
	    p->unknown_attributes->attr[n + 1] = htons(attr);

	n++;

	p->len += 4;

    } else {
	p->unknown_attributes->attr[n++] = htons(attr);
    }

    p->unknown_attributes->n = n;

    return 1;
}

int add_error_code(struct packet *p, int error)
{
    p->error_code = (struct error_code *) malloc
	(sizeof(struct error_code));

    p->error_code->errclass = error / 100;
    p->error_code->errnum = error % 100;
    p->error_code->reason = 0;

    p->len += 8;

    return 1;
}

#endif /* WITH_STUN_CLIENT */
