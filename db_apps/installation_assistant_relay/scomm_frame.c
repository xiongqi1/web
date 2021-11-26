/**
 * @file scomm_frame.c
 * @brief Implements a frame layer which is in charge of sending and receiving packets
 *        through the hardware driver
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
#include "scomm_datalink.h"
#include "scomm_frame.h"

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*******************************************************************************
 * Declare internal macros
 ******************************************************************************/

#define SCOMM_FRAME_END 0xC0
#define SCOMM_FRAME_ACK 0xD0
#define SCOMM_FRAME_ESC 0xDB
#define SCOMM_FRAME_ESC_END 0xDC
#define SCOMM_FRAME_ESC_ESC 0xDD
#define SCOMM_FRAME_ESC_ACK 0xDE

#define SCOMM_FRAME_ESC_RECEIVED 0x1

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int scomm_frame_init_packet(scomm_frame_t *frame);
static void scomm_frame_term_packet(scomm_frame_t *frame);
static void scomm_frame_process_rx_data(scomm_frame_t *frame, unsigned char data);
static int scomm_frame_data_received(void *param);
static int scomm_frame_write_byte(scomm_frame_t *frame, unsigned char data);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int scomm_frame_init(scomm_frame_t *frame, scomm_datalink_t *datalink,
					const scomm_driver_t *driver)
{
	frame->tx_chunk_len = 0;
	frame->tx_chunk = malloc(SCOMM_FRAME_CHUNK_LEN);
	if (!frame->tx_chunk) {
		return -1;
	}

	frame->datalink = datalink;
	frame->driver = driver;
	if (frame->driver) {
		if (frame->driver->set_data_received) {
			frame->driver->set_data_received(scomm_frame_data_received, frame);
		}
	}

	return 0;
}

void scomm_frame_term(scomm_frame_t *frame)
{
	scomm_frame_term_packet(frame);
	if (frame->tx_chunk) {
		free(frame->tx_chunk);
	}
}

int scomm_frame_start(scomm_frame_t *frame)
{
	return scomm_frame_init_packet(frame);
}

int scomm_frame_stop(scomm_frame_t *frame)
{
	/* Dummy interface */
	return 0;
}

int scomm_frame_send_data(scomm_frame_t *frame, scomm_packet_t *packet)
{
	int i;
	unsigned char data;
	unsigned char *data_ptr;
	unsigned char enc_data[2];
	int enc_data_len;
	int packet_len;
	int sent_len;
	int written_len;

	assert((frame->driver && frame->driver->write_bytes));

	written_len = sent_len = 0;
	data_ptr = scomm_packet_get_begin_pointer(packet);
	packet_len = scomm_packet_get_data_length(packet);
	LOG_INFO("scomm_frame_send_data: %p, %d\r\n", data_ptr, packet_len);
	while (packet_len > 0) {
		data = *data_ptr++;
		packet_len--;
		switch (data) {
		case SCOMM_FRAME_END:
			enc_data[0] = SCOMM_FRAME_ESC;
			enc_data[1] = SCOMM_FRAME_ESC_END;
			enc_data_len = 2;
			break;

		case SCOMM_FRAME_ESC:
			enc_data[0] = SCOMM_FRAME_ESC;
			enc_data[1] = SCOMM_FRAME_ESC_ESC;
			enc_data_len = 2;
			break;

		default:
			enc_data[0] = data;
			enc_data_len = 1;
			break;
		}

		for (i = 0; i < enc_data_len; i++) {
			scomm_frame_write_byte(frame, enc_data[i]);
			written_len++;
		}
		sent_len++;
	}

	/* Now, it's the end of frame */
	enc_data[0] = SCOMM_FRAME_END;
	scomm_frame_write_byte(frame, enc_data[0]);
	written_len++;

	/* Pad data until the size of written data is aligned with SCOMM_FRAME_ALIGN_BYTE_LEN */
	if (written_len % SCOMM_FRAME_ALIGN_BYTE_LEN) {
		int padding_len = SCOMM_FRAME_ALIGN_BYTE_LEN - (written_len % SCOMM_FRAME_ALIGN_BYTE_LEN);

		for (i = 0; i < padding_len; i++) {
			scomm_frame_write_byte(frame, enc_data[0]);
			written_len++;
		}
	}

	/*
	 * Notes: Sends additional align bytes to synchronise with NRB-0200 since it has trouble
	 *        in triggering DMA RX interrupt. These additional bytes improve the throughput
	 *        since NRB-0200 can make fully use of DMA operation.
	 */
	for (i = 0; i < SCOMM_FRAME_ALIGN_BYTE_LEN; i++) {
		scomm_frame_write_byte(frame, enc_data[0]);
		written_len++;
	}
	LOG_INFO("scomm_frame written: %d sent: %d\n", written_len, sent_len);

	return sent_len;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

static int scomm_frame_init_packet(scomm_frame_t *frame)
{
	frame->rx_flags = 0;
	frame->rx_packet = scomm_packet_create();
	if (!frame->rx_packet) {
		LOG_WARN("Allocating a packet failed\r\n");
		return -1;
	}

	return 0;
}

static void scomm_frame_term_packet(scomm_frame_t *frame)
{
	if (frame->rx_packet) {
		scomm_packet_destroy(frame->rx_packet);
	}
}

static void scomm_frame_process_rx_data(scomm_frame_t *frame, unsigned char data)
{
	if (!frame->rx_packet) {
		return ;
	}

	LOG_DEBUG("%02x:", data);
	switch (data) {
	case SCOMM_FRAME_END:
		if (scomm_packet_get_data_length(frame->rx_packet) > 0) {
			LOG_DEBUG("\r\n");
			scomm_datalink_data_received(frame->datalink, frame->rx_packet);
			scomm_frame_init_packet(frame);
		}
		break;

	case SCOMM_FRAME_ESC:
		frame->rx_flags |= SCOMM_FRAME_ESC_RECEIVED;
		break;

	case SCOMM_FRAME_ESC_END:
		if (frame->rx_flags & SCOMM_FRAME_ESC_RECEIVED) {
			scomm_packet_append_tail_uint8(frame->rx_packet, SCOMM_FRAME_END);
			frame->rx_flags &= ~SCOMM_FRAME_ESC_RECEIVED;
		} else {
			scomm_packet_append_tail_uint8(frame->rx_packet, data);
		}
		break;

	case SCOMM_FRAME_ESC_ESC:
		if (frame->rx_flags & SCOMM_FRAME_ESC_RECEIVED) {
			scomm_packet_append_tail_uint8(frame->rx_packet, SCOMM_FRAME_ESC);
			frame->rx_flags &= ~SCOMM_FRAME_ESC_RECEIVED;
		} else {
			scomm_packet_append_tail_uint8(frame->rx_packet, data);
		}
		break;

	default:
		scomm_packet_append_tail_uint8(frame->rx_packet, data);
		break;
	}
}

static int scomm_frame_data_received(void *param)
{
	unsigned char data;
	scomm_frame_t *frame = (scomm_frame_t *)param;

	assert((frame->driver && frame->driver->read_bytes));

	while (frame->driver->read_bytes(&data, 1) == 1) {
		scomm_frame_process_rx_data(frame, data);
	}

	return 0;
}

/**
 * @brief Write one byte to the lower layer.
 *
 * @param frame The context of the frame layer
 * @param data One byte data to be written to the driver
 * @return 0 on success, or a negative value on error.
 */
static int scomm_frame_write_byte(scomm_frame_t *frame, unsigned char data)
{
	assert((frame->driver && frame->driver->write_bytes));
	assert((frame->tx_chunk && (frame->tx_chunk_len < SCOMM_FRAME_CHUNK_LEN)));

	/* The one byte keeps in the queue until the size of queued data reaches SCOMM_FRAME_CHUNK_LEN */
	frame->tx_chunk[frame->tx_chunk_len++] = data;
	if (frame->tx_chunk_len >= SCOMM_FRAME_CHUNK_LEN) {
		frame->tx_chunk_len = 0;
		if (frame->driver->write_bytes(frame->tx_chunk, SCOMM_FRAME_CHUNK_LEN) != SCOMM_FRAME_CHUNK_LEN) {
			LOG_ERR("scomm frame writing failed\n");
			return -1;
		}
	}

	return 0;
}
