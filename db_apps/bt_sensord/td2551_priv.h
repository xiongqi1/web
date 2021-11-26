/*
 * td2551_priv.h:
 *    Taidoc 2551 Weight Scale device handling private declarations
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#ifndef __TD2551_PRIV_H__
#define __TD2551_PRIV_H__

#define FRAME_START_VAL 0x51
#define FRAME_STOP_VAL  0xa3

#define FRAME_CMD_ID_IDX     1
#define FRAME_DATA_START_IDX 2

#define CMD_GET_CLOCK_ID                 0x23
#define CMD_GET_CLOCK_REQ_LEN            8
#define CMD_GET_CLOCK_RES_LEN            8

#define CMD_READ_DEVICE_MODEL_ID         0x24
#define CMD_READ_DEVICE_MODEL_REQ_LEN    8
#define CMD_READ_DEVICE_MODEL_RES_LEN    8

#define CMD_READ_NUM_DATA_ID             0x2B
#define CMD_READ_NUM_DATA_REQ_LEN        8
#define CMD_READ_NUM_DATA_RES_LEN        8

#define CMD_SET_CLOCK_ID                 0x33
#define CMD_SET_CLOCK_REQ_LEN            8
#define CMD_SET_CLOCK_RES_LEN            8

#define CMD_TURN_OFF_ID                  0x50
#define CMD_TURN_OFF_REQ_LEN             8
#define CMD_TURN_OFF_RES_LEN             8

#define CMD_CLEAR_DATA_ID                0x52
#define CMD_CLEAR_DATA_REQ_LEN           8
#define CMD_CLEAR_DATA_RES_LEN           8

#define CMD_READ_DATA_ID                 0x71
#define CMD_READ_DATA_REQ_LEN            7
#define CMD_READ_DATA_RES_LEN            34

#endif /* __TD2551_PRIV_H__ */
