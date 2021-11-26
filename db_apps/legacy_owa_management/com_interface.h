/**
 * @file com_interface.h
 * @brief legacy OWA backward compatible communication interface
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

#ifndef COM_INTERFACE_H_11020009122019
#define COM_INTERFACE_H_11020009122019
#include "message_protocol.h"

#define MANAGER_FIFO_R "/tmp/fifo_spm_mgmt"
#define MANAGER_FIFO_W "/tmp/fifo_mgmt_spm"

#define MSG_LEN_BYTES 2  //used for fifo channel as real message length

/*
 * Communication channels initialize
 *
 * @return 0 on success; non-zero error code on failure
 */
int com_channels_init();

/*
 * Communication channels close
 * release fifo handler
 */
void com_channels_close();

/*
 * get read fifo file handler
 * @return read fifo file hander; -1 on failure
 */
int com_get_read_fd(void);

/*
 * get write fifo file handler
 * @return write fifo file hander; -1 on failure
 */
int com_get_write_fd(void);

/*
 * read message from read fifo channel
 * @param ptr point to char type buffer which will be stored the bytes read from fifo read channel
 * @return none-zero number for read back bytes on success; -1 on failure
 */
int read_message(char * ptr);

/*
 *  write message to write fifo channel
 * @param packet point to mgmt_packet_t type buffer which will be sent through write channel
 * @return none-zero number for written bytes on success; -1 on failure
 */
int write_message(mgmt_packet_t * packet);

#endif