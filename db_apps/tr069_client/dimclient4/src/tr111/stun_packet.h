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

#ifndef PACKET_H
#define PACKET_H

#include "globals.h"

#ifdef WITH_STUN_CLIENT

#define STUN_MT_BINREQ 0x0001
#define STUN_MT_BINRES 0x0101
#define STUN_MT_BINERR 0x0111
#define STUN_MT_SHAREQ 0x0002
#define STUN_MT_SHARES 0x0102
#define STUN_MT_SHAERR 0x0112

#define STUN_AT_MAPADD 0x0001
#define STUN_AT_RESADD 0x0002
#define STUN_AT_CHGREQ 0x0003
#define STUN_AT_SRCADD 0x0004
#define STUN_AT_CHGADD 0x0005
#define STUN_AT_USRNAM 0x0006
#define STUN_AT_PASSWD 0x0007
#define STUN_AT_MSGINT 0x0008
#define STUN_AT_ERRCOD 0x0009
#define STUN_AT_UNKATT 0x000a
#define STUN_AT_REFFRM 0x000b

// San 04 june 2011:
#define STUN_CONNECTION_REQUEST_BIN_ATTR 0xC001
#define STUN_BINDING_CHANGE_ATTR 0xC002

#define STUN_ERR_BAD_REQ 	400
#define STUN_ERR_UNKNOWN_ATTR 	420

typedef struct stun_tid {
    unsigned char data[16];
} stun_tid_t;

struct address {
    u_int8_t family;
    u_int16_t port;
    u_int32_t address;
};

struct stun_string {
    char *data;
    int len;
};

/* fix this */
struct request {
    u_int8_t a;
    u_int8_t b;
};

struct error_code {
    u_int8_t errclass;
    u_int8_t errnum;
    char *reason;
};

struct attr_list {
    u_int16_t *attr;
    int n;
};

struct packet {
    u_int16_t type;
    u_int16_t len;
    stun_tid_t tid;
    struct address *mapped_address;
    struct address *response_address;
    struct request *change_request;
    struct address *source_address;
    struct address *changed_address;
    struct stun_string *username;
    struct stun_string *password;
    u_int8_t *message_integrity;
    struct error_code *error_code;
    struct attr_list *unknown_attributes;
    struct address *reflected_from;

    // San 04 june 2011:
    u_int16_t * binding_change_attr;
    u_int16_t * connection_request_binding_attr;
};

struct packet_s {
    char *data;
    int len;
};

struct packet *packet_create(u_int16_t, stun_tid_t);
int add_mapped_address(struct packet *, u_int32_t, u_int16_t);
int add_unknown_attribute(struct packet *, u_int16_t);
int add_response_address(struct packet *, u_int32_t, u_int16_t);
int add_changed_address(struct packet *, u_int32_t, u_int16_t);
int add_source_address(struct packet *, u_int32_t, u_int16_t);
int add_reflected_from(struct packet *, u_int32_t, u_int16_t);
int add_username(struct packet *, char *);
int add_change_request(struct packet *, unsigned char, unsigned char);
struct packet_s *serialise_packet(struct packet *p);
struct packet *packet_parse(unsigned char *buf, int n);
void destroy_packet(struct packet *);
void destroy_packet_s(struct packet_s *);
void create_random_tid(stun_tid_t *);
int add_error_code(struct packet *, int);

// San 04 june 2011:
int add_binding_change_attr(struct packet *p);
int add_connection_request_binding_attr(struct packet *p);

#endif /* WITH_STUN_CLIENT */

#endif /* PACKET_H */
