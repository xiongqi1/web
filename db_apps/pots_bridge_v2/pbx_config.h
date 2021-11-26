#ifndef __PBX_CONFIG_H_20180620__
#define __PBX_CONFIG_H_20180620__

/*
 * PBX settings.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#define SI3217XB_NUMBER_OF_DEVICE 1 /* Same for 17x C */
#define SI3218X_NUMBER_OF_DEVICE 1
#define SI3219X_NUMBER_OF_DEVICE 1
#define SI3226X_NUMBER_OF_DEVICE 1
#define SI3228X_NUMBER_OF_DEVICE 1
#define SI3050_NUMBER_OF_DEVICE  1
#define PBX_PORT_COUNT 1
#define MAX_NUMBER_OF_DEVICE 16
#define MAX_NUMBER_OF_CHAN 32

/* If manual ring cadence code is included, how may periods to support... */
#define PROSLIC_MAX_RING_PERIODS  2

#define PBX_PORT_COUNT           1
#define DEFAULT_CID_FSK_NA        0
#define DEFAULT_CID_RING_NA       0
#define PBX_PCM_PRESET            PCM_16LIN
#define PCM_PRESET_NUM_8BIT_TS    2 /* 1 = uLaw/aLaw, 2 = 16 bit linear */
#define PROSLIC_MAX_RING_PERIODS  2
#define PBX_MWI_ON_TIME           500 /* in mSec */
#define PBX_MWI_OFF_TIME          1000 /* in mSec */

#endif

