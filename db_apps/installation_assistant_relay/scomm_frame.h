#ifndef SCOMM_FRAME_H_10104101122015
#define SCOMM_FRAME_H_10104101122015
/**
 * @file scomm_frame.h
 * @brief Provides public interfaces to use the frame layer
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

/*******************************************************************************
 * Declare public macros
 ******************************************************************************/

/*
 * All scomm frame should be 64-byte aligned, if the size of the packet is not
 * 64-byte aligned, '0xC0' is padded to the packet until it is aligned.
 */
#define SCOMM_FRAME_ALIGN_BYTE_LEN 64

/*
 * The size of chunk indicates how many number of bytes is buffered until it
 * finally passes to the serial driver. The size should be SCOMM_FRAME_ALIGN_BYTE_LEN
 * divided by N, where N is a positive natural number.
 */
#define SCOMM_FRAME_CHUNK_LEN (SCOMM_FRAME_ALIGN_BYTE_LEN / 4)

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

/**
 * @brief The structure of the main frame layer
 */
struct scomm_frame {
	scomm_datalink_t *datalink;
	const scomm_driver_t *driver;
	int rx_flags;
	scomm_packet_t *rx_packet;
	int rx_acked_len;
	unsigned char *tx_chunk;
	int tx_chunk_len;
};

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialise the data link layer
 *
 * @param frame The context of the frame layer
 * @param service The pointer to the data link layer
 * @param driver The driver that can be used to have access the communication driver.
 * @return Void
 */
int scomm_frame_init(scomm_frame_t *frame, scomm_datalink_t *datalink,
					const scomm_driver_t *driver);

/**
 * @brief Terminates the frame layer
 *
 * @param datalink The context of the data link
 * @return Void
 */
void scomm_frame_term(scomm_frame_t *frame);

/**
 * @brief Starts the frame layer
 *
 * @param frame The context of the frame layer
 * @return 0 on success, or a negative value on error
 */
int scomm_frame_start(scomm_frame_t *frame);

/**
 * @brief Stops the frame layer
 *
 * @param frame The context of the frame layer
 * @return 0 on success, or a negative value on error
 */
int scomm_frame_stop(scomm_frame_t *frame);

/**
 * @brief Encodes frame information on the packet and delivers it to the communication driver
 *
 * @param frame The context of the frame layer
 * @return 0 on success, or a negative value on error
 */
int scomm_frame_send_data(scomm_frame_t *frame, scomm_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
