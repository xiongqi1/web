#ifndef SCOMM_PACKET_H_10365301122015
#define SCOMM_PACKET_H_10365301122015
/**
 * @file scomm_packet.h
 * @brief provides public unsigned interfaces to use the scomm_packet module
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

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

typedef struct scomm_packet scomm_packet_t;

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Allocates a packet
 *
 * @return The pointer to the packet on success, NULL othewise
 */
scomm_packet_t *scomm_packet_create(void);

/**
 * @brief Deallocates the packet
 *
 * @param packet The packet to be deallocated
 * @return Void
 */
void scomm_packet_destroy(scomm_packet_t *packet);

/**
 * @brief Gets the pointer to the beginning of data in the packet
 *
 * @param packet The packet to be used
 * @return The pointer to data on success, NULL otherwise
 */
unsigned char *scomm_packet_get_begin_pointer(scomm_packet_t *packet);

/**
 * @brief Gets the pointer to the end of data in the packet
 *
 * @param packet The packet to be used
 * @return The pointer to data on success, NULL otherwise
 */
unsigned char *scomm_packet_get_end_pointer(scomm_packet_t *packet);

/**
 * @brief Gets the number of bytes written in the packet
 *
 * @param packet The packet to be used
 * @return The number of bytes written in the packet.
 */
int scomm_packet_get_data_length(scomm_packet_t *packet);

/**
 * @brief Gets the number of bytes available in the head of the packet
 *
 * @param packet The packet to be used
 * @return The number of bytes available in the head of the packet
 */
int scomm_packet_get_head_avail_length(scomm_packet_t *packet);

/**
 * @brief Gets the number of bytes available in the tail of the packet
 *
 * @param packet The packet to be used
 * @return The number of bytes available in the tail of the packet
 */
int scomm_packet_get_tail_avail_length(scomm_packet_t *packet);

/**
 * @brief Appends data in the head of the packet
 *
 * @param packet The packet where data is appended
 * @param data The location of data to be appended
 * @param len The number of data in bytes
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_head_bytes(scomm_packet_t *packet, const unsigned char *data, int len);

/**
 * @brief Appends data in the tail of the packet
 *
 * @param packet The packet where data is appended
 * @param data The location of data to be appended
 * @param len The number of data in bytes
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_tail_bytes(scomm_packet_t *packet, const unsigned char *data, int len);

/**
 * @brief Removes data from the head of the packet
 *
 * @param packet The packet where data is removed
 * @param data The location of data to be removed
 * @param len The number of data in bytes
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_head_bytes(scomm_packet_t *packet, unsigned char *data, int len);

/**
 * @brief Removes data from the tail of the packet
 *
 * @param packet The packet where data is removed
 * @param data The location of data to be removed
 * @param len The number of data in bytes
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_tail_bytes(scomm_packet_t *packet, unsigned char *data, int len);

/**
 * @brief Appends data in the head of the packet
 *
 * @param packet The packet where data is appended
 * @param data The 1 byte data to be appended
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_head_uint8(scomm_packet_t *packet, unsigned char data);

/**
 * @brief Appends data in the tail of the packet
 *
 * @param packet The packet where data is appended
 * @param data The 1 byte data to be appended
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_tail_uint8(scomm_packet_t *packet, unsigned char data);

/**
 * @brief Removes data in the head of the packet
 *
 * @param packet The packet where data is removed
 * @param data The 1 byte data to be removed
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_head_uint8(scomm_packet_t *packet, unsigned char *data);

/**
 * @brief Removes data in the tail of the packet
 *
 * @param packet The packet where data is removed
 * @param data The 1 byte data to be removed
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_tail_uint8(scomm_packet_t *packet, unsigned char *data);

/**
 * @brief Appends data in the head of the packet
 *
 * @param packet The packet where data is appended
 * @param data The 2 bytes data to be appended
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_head_uint16(scomm_packet_t *packet, unsigned short data);

/**
 * @brief Appends data in the tail of the packet
 *
 * @param packet The packet where data is appended
 * @param data The 2 bytes data to be appended
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_tail_uint16(scomm_packet_t *packet, unsigned short data);

/**
 * @brief Removes data in the head of the packet
 *
 * @param packet The packet where data is removed
 * @param data The 2 bytes data to be removed
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_head_uint16(scomm_packet_t *packet, unsigned short *data);

/**
 * @brief Removes data in the tail of the packet
 *
 * @param packet The packet where data is removed
 * @param data The 2 bytes data to be removed
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_tail_uint16(scomm_packet_t *packet, unsigned short *data);

/**
 * @brief Appends data in the head of the packet
 *
 * @param packet The packet where data is appended
 * @param data The 4 bytes data to be appended
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_head_uint32(scomm_packet_t *packet, unsigned int data);

/**
 * @brief Appends data in the tail of the packet
 *
 * @param packet The packet where data is appended
 * @param data The 4 bytes data to be appended
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_append_tail_uint32(scomm_packet_t *packet, unsigned int data);

/**
 * @brief Removes data in the head of the packet
 *
 * @param packet The packet where data is removed
 * @param data The 4 bytes data to be removed
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_head_uint32(scomm_packet_t *packet, unsigned int *data);

/**
 * @brief Removes data in the tail of the packet
 *
 * @param packet The packet where data is removed
 * @param data The 4 bytes data to be removed
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_remove_tail_uint32(scomm_packet_t *packet, unsigned int *data);

/**
 * @brief Moves the position of the head forward in the packet
 *
 * @param packet The packet where data is skipped
 * @param len The number of bytes to be forwarded
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_forward_head_bytes(scomm_packet_t *packet, int len);


/**
 * @brief Moves the position of the head backward in the packet
 *
 * @param packet The packet where data is skipped
 * @param len The number of bytes to be backwarded
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_backward_head_bytes(scomm_packet_t *packet, int len);

/**
 * @brief Moves the position of the tail forward in the packet
 *
 * @param packet The packet where data is skipped
 * @param len The number of bytes to be forwarded
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_forward_tail_bytes(scomm_packet_t *packet, int len);

/**
 * @brief Moves the position of the tail backward in the packet
 *
 * @param packet The packet where data is skipped
 * @param len The number of bytes to be backwarded
 * @return 0 on success, or a negative value on error.
 */
int scomm_packet_backward_tail_bytes(scomm_packet_t *packet, int len);

/**
 * @brief Prints the content of the packet
 *
 * @param packet The packet to be printed
 * @return Void
 */
void scomm_packet_print(scomm_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
