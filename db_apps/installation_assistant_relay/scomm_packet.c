/**
 * @file scomm_packet.c
 * @brief Implements helper functions to manage packets and put/get data from/to them.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "scomm.h"
#include "scomm_packet.h"

#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*******************************************************************************
 * Declare internal macros
 ******************************************************************************/

#define MAX_SCOMM_PACKET_HEAD_ROOM_SIZE 12
#define MAX_SCOMM_PACKET_TAIL_ROOM_SIZE 4

#define MAX_SCOMM_PACKET_BUFFER_SIZE (MAX_SCOMM_PACKET_SIZE + \
									MAX_SCOMM_PACKET_HEAD_ROOM_SIZE + \
									MAX_SCOMM_PACKET_TAIL_ROOM_SIZE)

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

struct scomm_packet {
	unsigned short data;
	unsigned short tail;
	unsigned char buffer[MAX_SCOMM_PACKET_BUFFER_SIZE];
};

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

static int num_alloced_packets = 0;

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

scomm_packet_t *scomm_packet_create(void)
{
	scomm_packet_t *packet = malloc(sizeof(scomm_packet_t));

	if (packet) {
		packet->data = MAX_SCOMM_PACKET_HEAD_ROOM_SIZE;
		packet->tail = packet->data;
	} else {
		LOG_ERR("scomm_packet_alloc fails:%d\r\n", num_alloced_packets);
	}

	num_alloced_packets++;
	return packet;
}

void scomm_packet_destroy(scomm_packet_t *packet)
{
	if (packet) {
		num_alloced_packets--;
		free(packet);
	}
}

unsigned char *scomm_packet_get_begin_pointer(scomm_packet_t *packet)
{
	return &packet->buffer[packet->data];
}

unsigned char *scomm_packet_get_end_pointer(scomm_packet_t *packet)
{
	return &packet->buffer[packet->tail];
}

int scomm_packet_get_data_length(scomm_packet_t *packet)
{
	assert((packet && (packet->tail >= packet->data)));
	return (packet->tail - packet->data);
}

int scomm_packet_get_head_avail_length(scomm_packet_t *packet)
{
	return packet->data;
}

int scomm_packet_get_tail_avail_length(scomm_packet_t *packet)
{
	return (MAX_SCOMM_PACKET_BUFFER_SIZE - packet->tail);
}

int scomm_packet_append_head_bytes(scomm_packet_t *packet, const unsigned char *data, int len)
{
	if (len && (packet->data >= len)) {
		packet->data -= len;
		memcpy(&packet->buffer[packet->data], data, len);
		return 0;
	}

	LOG_ERR("Adding head: (%d, %d, %d)\r\n", packet->data, packet->tail, len);
	return -1;
}

int scomm_packet_append_tail_bytes(scomm_packet_t *packet, const unsigned char *data, int len)
{
	if (len && ((packet->tail + len) <= MAX_SCOMM_PACKET_BUFFER_SIZE)) {
		memcpy(&packet->buffer[packet->tail], data, len);
		packet->tail += len;
		return 0;
	}

	LOG_ERR("Adding tail: (%p, %d, %d, %d)\r\n", packet, packet->data, packet->tail, len);
	return -1;
}

int scomm_packet_remove_head_bytes(scomm_packet_t *packet, unsigned char *data, int len)
{
	if (len && ((packet->data + len) <= packet->tail)) {
		memcpy(data, &packet->buffer[packet->data], len);
		packet->data += len;
		return 0;
	}

	LOG_ERR("Removing head: (%d, %d, %d)\r\n", packet->data, packet->tail, len);
	return -1;
}

int scomm_packet_remove_tail_bytes(scomm_packet_t *packet, unsigned char *data, int len)
{
	if (len && ((packet->data + len) <= packet->tail)) {
		packet->tail -= len;
		memcpy(data, &packet->buffer[packet->tail], len);
		return 0;
	}

	LOG_ERR("Removing tail: (%d, %d, %d)\r\n", packet->data, packet->tail, len);
	return -1;
}

int scomm_packet_forward_head_bytes(scomm_packet_t *packet, int len)
{
	if (len && ((packet->data + len) <= packet->tail)) {
		packet->data += len;
		return 0;
	}

	return -1;
}

int scomm_packet_backward_head_bytes(scomm_packet_t *packet, int len)
{
	if (len && (packet->data >= len)) {
		packet->data -= len;
		return 0;
	}

	return -1;
}

int scomm_packet_forward_tail_bytes(scomm_packet_t *packet, int len)
{
	if (len && ((packet->tail + len) <= MAX_SCOMM_PACKET_BUFFER_SIZE)) {
		packet->tail += len;
		return 0;
	}

	return -1;
}

int scomm_packet_backward_tail_bytes(scomm_packet_t *packet, int len)
{
	if (len && ((packet->data + len) <= packet->tail)) {
		packet->tail -= len;
		return 0;
	}

	return -1;
}

int scomm_packet_append_head_uint8(scomm_packet_t *packet, unsigned char data)
{
	return scomm_packet_append_head_bytes(packet, &data, 1);
}

int scomm_packet_append_tail_uint8(scomm_packet_t *packet, unsigned char data)
{
	return scomm_packet_append_tail_bytes(packet, &data, 1);
}

int scomm_packet_remove_head_uint8(scomm_packet_t *packet, unsigned char *data)
{
	return scomm_packet_remove_head_bytes(packet, data, 1);
}

int scomm_packet_remove_tail_uint8(scomm_packet_t *packet, unsigned char *data)
{
	return scomm_packet_remove_tail_bytes(packet, data, 1);
}

int scomm_packet_append_head_uint16(scomm_packet_t *packet, unsigned short data)
{
	return scomm_packet_append_head_bytes(packet, (const unsigned char *)&data, 2);
}

int scomm_packet_append_tail_uint16(scomm_packet_t *packet, unsigned short data)
{
	return scomm_packet_append_tail_bytes(packet, (const unsigned char *)&data, 2);
}

int scomm_packet_remove_head_uint16(scomm_packet_t *packet, unsigned short *data)
{
	return scomm_packet_remove_head_bytes(packet, (unsigned char *)data, 2);
}

int scomm_packet_remove_tail_uint16(scomm_packet_t *packet, unsigned short *data)
{
	return scomm_packet_remove_tail_bytes(packet, (unsigned char *)data, 2);
}

int scomm_packet_append_head_uint32(scomm_packet_t *packet, unsigned int data)
{
	return scomm_packet_append_head_bytes(packet, (const unsigned char *)&data, 4);
}

int scomm_packet_append_tail_uint32(scomm_packet_t *packet, unsigned int data)
{
	return scomm_packet_append_tail_bytes(packet, (const unsigned char *)&data, 4);
}

int scomm_packet_remove_head_uint32(scomm_packet_t *packet, unsigned int *data)
{
	return scomm_packet_remove_head_bytes(packet, (unsigned char *)data, 4);
}

int scomm_packet_remove_tail_uint32(scomm_packet_t *packet, unsigned int *data)
{
	return scomm_packet_remove_tail_bytes(packet, (unsigned char *)data, 4);
}

void scomm_packet_print(scomm_packet_t *packet)
{
#if 0
	int i;
	int length;
	unsigned char *data, *end;

	data = scomm_packet_get_begin_pointer(packet);
	end = scomm_packet_get_end_pointer(packet);
	length = scomm_packet_get_data_length(packet);

	LOG_ERR(">>L:%d,D:%p,E:%p#", length, data, end);
	for (i = 0; i < length; i++) {
		if (!(i % 16)) {
			LOG_ERR("\r\n%02x:", data[i]);
		} else {
			LOG_ERR("%02x:", data[i]);
		}
	}
	LOG_ERR("<<\r\n");
#endif

}
