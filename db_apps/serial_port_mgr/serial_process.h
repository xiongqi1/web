/**
 * @file serial_process.h
 * @brief L1/L2 procesing for serial port
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
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

#ifndef SERIAL_PROCESS_H_10253120082019
#define SERIAL_PROCESS_H_10253120082019

#include <stdio.h>
#include <fifo_process.h>

#define L1_FRAME_BASE_SIZE 64

#define SCOMM_DATALINK_PROTO_VERSION        1
#define SCOMM_DATALINK_PROTO_VERSION_POS    0
#define SCOMM_DATALINK_PROTO_VERSION_MASK   0x7

#define SCOMM_DATALINK_PROTO_SERVICE_ID_POS     3
#define SCOMM_DATALINK_PROTO_SERVICE_ID_MASK    0x1f

/**
 * Read data from serial port and process data
 *
 * @param serial Serial port file descriptor
 * @param mgmt Management service FIFO
 * @param socks Sockets service FIFO
 */
void serial_process_data(int serial, FifoProcess* mgmt, FifoProcess* socks);

/**
 * Encode L1 data and write to serial port after
 *
 * @param serial Serial port file pointer
 * @param data Data to be sent
 * @param len Data length
 */
void serial_send_data(FILE* serial, const unsigned char* data, int len);

#endif // SERIAL_PROCESS_H_10253120082019
