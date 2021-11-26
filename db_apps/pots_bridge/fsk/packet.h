/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */


#ifndef FSK_PACKET_H_20090309_
#define FSK_PACKET_H_20090309_

#include "slic/fsk.h"

typedef enum
{
	  fsk_packet_sdmf_header = 0x04
	, fsk_packet_mdmf_header = 0x80
	, fsk_packet_mdmf_test_sequence_header = 0x81
	, fsk_packet_message_waiting_notification = 0x82
	, fsk_packet_message_waiting_indicator = 0x06
	, fsk_packet_reserved = 0x0B
	// etc
} fsk_packet_type;

// quick and dirty; we make it better, if we need mdmf
fsk_packet* fsk_packet_init( fsk_packet* packet, fsk_packet_type type, const char* payload, unsigned char size );
fsk_packet* fsk_packet_clip( fsk_packet* packet, const char* clip );
fsk_packet* vmwi_packet_clip(fsk_packet* packet, int on);

#endif /* PACKET_H_20090309_ */
