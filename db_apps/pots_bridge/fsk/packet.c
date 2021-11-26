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


#include <string.h>
#include <stdio.h>
#include <time.h>
#include "cdcs_syslog.h"
#include "./packet.h"

static unsigned char checksum_(unsigned char initial_checksum, const char* begin, const char* end)
{
	unsigned int sum = (0x0100 - initial_checksum) & 0xff;
	const char* it;
	for (it = begin; it < end; ++it)
	{
		sum += (unsigned int)(*it);
	}
	return 0x0100 - (sum & 0x00ff);
}

fsk_packet* fsk_packet_init(fsk_packet* packet, fsk_packet_type type, const char* payload, unsigned char size)
{
	packet->size = size + 3;
	packet->data[0] = type;
	packet->data[1] = size;
	if (&packet->data[2] != payload)
	{
		memcpy(&packet->data[2], payload, size);
	}
	packet->data[ packet->size -1 ] = checksum_(0, packet->data, packet->data + packet->size - 1);
	return packet;
}

//#define TEST_CODE_FOR_CANADIAN_CLIP
#ifdef TEST_CODE_FOR_CANADIAN_CLIP
fsk_packet* fsk_packet_clip(fsk_packet* packet, const char* clip)
{
	char test_clip[] = "+5551212";
	int len = strlen(test_clip);
	unsigned char size = len +  8;
	char* payload = packet->data + FSK_PACKET_PAYLOAD_OFFSET;
	sprintf(payload, "%s%s%s%s%s", "04", "01", "16", "36", (test_clip[0] == '+')? &test_clip[1]:test_clip);
	if (test_clip[0] == '+') {
		len -= 1;
		size = len +  8;
	}
	SYSLOG_DEBUG("clip number: '%s'", (test_clip[0] == '+')? &test_clip[1]:test_clip);
	SYSLOG_DEBUG("payload: '%s', len %d", payload, len);
	fsk_packet_init(packet, fsk_packet_sdmf_header, payload, size);
	return packet;
}
#else
fsk_packet* fsk_packet_clip(fsk_packet* packet, const char* clip)
{
	int len = strlen(clip);
	unsigned char size = len +  8;
	char* payload = packet->data + FSK_PACKET_PAYLOAD_OFFSET;
	time_t t = time(NULL);
	struct tm* s = localtime(&t);
	if (s == 0)
	{
		sprintf(payload, "00000000%s", *clip ? ((clip[0] == '+') ? clip+1:clip) : "P");
	}
	else
	{
		sprintf(payload, "%02d%02d%02d%02d%s", s->tm_mon + 1, s->tm_mday, s->tm_hour, s->tm_min, *clip ? ((clip[0] == '+') ? clip+1:clip) : "P");
	}
	if (clip[0] == '+') {
		len -= 1;
		size = len +  8;
	}
	SYSLOG_DEBUG("clip number: '%s'", (clip[0] == '+')? clip+1:clip);
	SYSLOG_DEBUG("payload: '%s', len %d", payload, len);
	fsk_packet_init(packet, fsk_packet_sdmf_header, payload, size);
	return packet;
}
#endif

#define VMWI_PKT_SIZE	6
fsk_packet* vmwi_packet_clip(fsk_packet* packet, int on)
{
	char turn_on_pkt[VMWI_PKT_SIZE] =  { 0x82, 0x03, 0x0B, 0x01, 0xFF, 0x70};
	char turn_off_pkt[VMWI_PKT_SIZE] = { 0x82, 0x03, 0x0B, 0x01, 0x00, 0x6F};
	SYSLOG_DEBUG("make VMWI indicator %s packet", (on)? "ON" : "OFF");
	(void) memset(packet->data, 0x00, FSK_PACKET_SIZE_MAX);
	if (on)
		(void) memcpy(packet->data, (const char *)&turn_on_pkt[0], VMWI_PKT_SIZE);
	else
		(void) memcpy(packet->data, (const char *)&turn_off_pkt[0], VMWI_PKT_SIZE);
	packet->size = VMWI_PKT_SIZE;
	return packet;
}
