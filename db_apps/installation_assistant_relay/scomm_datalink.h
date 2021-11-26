#ifndef SCOMM_DATALINK_H_09561201122015
#define SCOMM_DATALINK_H_09561201122015
/**
 * @file scomm_datalink.h
 * @brief Provides public interfaces to use the data link layer
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
#include "scomm_frame.h"

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

struct scomm_datalink {
    scomm_frame_t frame;
    scomm_service_t *service;
};

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Allocates an instance of data link layer
 *
 * @return A data link instance on success, NULL otherwise
 */
scomm_datalink_t *scomm_datalink_create(void);

/**
 * @brief Deallocates an instance of data link layer
 *
 * @param datalink The context of the data link
 * @return Void
 */
void scomm_datalink_destroy(scomm_datalink_t *datalink);

/**
 * @brief Initialises the data link layer
 *
 * @param datalink The context of the data link
 * @param service The pointer to the scomm service
 * @param driver The driver that can be used to have access the communication driver.
 * @return Void
 */
int scomm_datalink_init(scomm_datalink_t *datalink, scomm_service_t *service,
						const scomm_driver_t *driver);

/**
 * @brief Terminates the data link layer
 *
 * @param datalink The context of the data link
 * @return Void
 */
void scomm_datalink_term(scomm_datalink_t *datalink);

/**
 * @brief Starts the data link layer
 *
 * @param datalink The context of the data link
 * @return 0 on success, or a negative value on error
 */
int scomm_datalink_start(scomm_datalink_t *datalink);

/**
 * @brief Stops the data link layer
 *
 * @param datalink The context of the data link
 * @return 0 on success, or a negative value on error
 */
int scomm_datalink_stop(scomm_datalink_t *datalink);

/**
 * @brief Called when data is available for the data link layer
 *
 * @param datalink The context of the data link
 * @param packet The packet carrying the payload
 * @return The number of data in bytes on success, or a negative value on error
 */
int scomm_datalink_data_received(scomm_datalink_t *datalink, scomm_packet_t *packet);

/**
 * @brief Encodes data link information on the packet and delivers it to the frame layer.
 *
 * @param datalink The context of the data link
 * @param id The service id
 * @param packet The packet where data will be written.
 * @return The number of bytes to be written on success, or a negative value on error
 */
int scomm_datalink_send_data(scomm_datalink_t *datalink, int id, scomm_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
