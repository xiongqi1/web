#ifndef SCOMM_H_11045701122015
#define SCOMM_H_11045701122015
/**
 * @file scomm.h
 * @brief Provides a collection of external header files to export them to applications
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
#include "scomm_packet.h"

#ifndef MAX_SCOMM_SERVICE_NUM
#define MAX_SCOMM_SERVICE_NUM 32
#endif

/**
 * The bigger size is better from the throughput point of view
 * The smaller size is better from the stablility point of view
 * This value should be agreed with NRB-0200
 */
#ifndef MAX_SCOMM_PACKET_SIZE
#define MAX_SCOMM_PACKET_SIZE 800
#endif

typedef struct scomm_service scomm_service_t;
typedef struct scomm_datalink scomm_datalink_t;
typedef struct scomm_frame scomm_frame_t;

/**
 * @brief The structure of the TLV
 */
typedef struct scomm_tlv {
	unsigned char type;
	unsigned char length;
	unsigned char data[];
} __attribute__((packed)) scomm_tlv_t;

/**
 * @brief The structure of driver interface to use the hardware driver
 */
typedef struct scomm_driver {
	int (*read_bytes)(unsigned char *data, int len);
	int (*write_bytes)(unsigned char *data, int len);
	int (*set_data_received)(int (*data_received)(void *param), void *param);
} scomm_driver_t;

/**
 * @brief The structure of the client for the scomm service
 */
typedef struct scomm_service_client {
	int service_id;
	int (*data_received_cb)(scomm_packet_t *packet, void *owner);
	void *owner;
} scomm_service_client_t;

/**
 * @brief Initialises scomm module
 *
 * @return 0 on success, or a negative value on error
 */
int scomm_init(void);

/**
 * @brief Terminates scomm module
 *
 * @return Void
 */
void scomm_term(void);

/**
 * @brief Allocates a packet
 *
 * @return The pointer to the packet on success, or NULL on error
 */
scomm_packet_t *scomm_alloc_packet(void);

/**
 * @brief Free the packet
 *
 * @param packet The pointer to the packet
 * @return The pointer to the packet on success, or NULL on error
 */
void scomm_free_packet(scomm_packet_t *packet);

#endif
