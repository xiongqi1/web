#include <string.h>
#include <stdlib.h>
#include <alloca.h>
#include <stdint.h>

#include "../dyna.h"
#include "../at/at.h"
#include "../util/at_util.h"
#include "pdu.h"
#include "sms.h"
#include "coding.h"
#include "gsm7.h"
#include "utf8.h"
#include "cdcs_syslog.h"

///////////////////////////////////////////////////////////////////////////////
/*static int pduConvSemiBCDOctToInt(unsigned char bSemiBCDOct)
{
	return ((bSemiBCDOct >> 4) & 0x0f) * 10 + (bSemiBCDOct&0x0f);
}*/
///////////////////////////////////////////////////////////////////////////////
static int pduConvSemiBCDOctToIntReverse(unsigned char bSemiBCDOct)
{
	return ((bSemiBCDOct >> 4) & 0x0f) + (bSemiBCDOct & 0x0f) * 10;
}
///////////////////////////////////////////////////////////////////////////////
int pduConvCharNibbleToInt(char chNibble)
{
	if (('0' <= chNibble) && (chNibble <= '9'))
		return chNibble -'0';

	return ((chNibble | ('a' ^ 'A')) - 'a' + 0x0a) & 0x0f;
}
///////////////////////////////////////////////////////////////////////////////
void* pduCreateOctByStr(const char* szStrHex, int* pIntHexLen)
{
	int cbIntHex;

	cbIntHex = (strlen(szStrHex) + 1) / 2 + 1; /* including last null */
	char* pIntHex = dynaCreate(cbIntHex, 0);

	int iInt = 0;

	while (*szStrHex)
	{
		// get high nibble
		int nHiNibble = pduConvCharNibbleToInt(*szStrHex++);

		// get low nibble
		int nLoNibble = 0;
		if (*szStrHex)
			nLoNibble = pduConvCharNibbleToInt(*szStrHex++);

		pIntHex[iInt++] = nHiNibble << 4 | nLoNibble;
	}

	*pIntHexLen = cbIntHex;

	return pIntHex;
}
///////////////////////////////////////////////////////////////////////////////
static int pduConvIntNibbleToChar(int nNibble)
{
	if (nNibble >= 0x0a)
		return nNibble -0x0a + 'A';

	return nNibble + '0';
}
///////////////////////////////////////////////////////////////////////////////
int pduConvSemiOctToStr(const char* halfOcts, int cbHalfOcts, char* achBuf, int cbBuf)
{
	int iBuf = 0;
	int nLoHOct, nHiHOct;

	while (iBuf < cbBuf && iBuf < cbHalfOcts)
	{
		nLoHOct = halfOcts[iBuf/2] & 0x0f;
		nHiHOct = (halfOcts[iBuf/2] >> 4) & 0x0f;

		// put low oct first
		achBuf[iBuf++] = pduConvIntNibbleToChar(nLoHOct);
		if (iBuf >= cbBuf || iBuf >= cbHalfOcts || nHiHOct == 0x0f)
			break;
		// put high oct
		achBuf[iBuf++] = pduConvIntNibbleToChar(nHiHOct);
	}

	if (iBuf < cbBuf)
	{
		achBuf[iBuf] = 0;
		return strlen(achBuf);
	}

	achBuf[cbBuf-1] = 0;

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int pduConvHexOctToStr(const char* HexOcts, int cbHexOcts, char* achBuf, int cbBuf, BOOL high_first)
{
	int iBuf = 0;
	int nLoHOct, nHiHOct;

	while (iBuf < cbBuf && iBuf < cbHexOcts)
	{
		nLoHOct = HexOcts[iBuf/2] & 0x0f;
		nHiHOct = (HexOcts[iBuf/2] >> 4) & 0x0f;
		if (high_first)
			achBuf[iBuf++] = pduConvIntNibbleToChar(nHiHOct);
		else
			achBuf[iBuf++] = pduConvIntNibbleToChar(nLoHOct);
		if (iBuf >= cbBuf || iBuf >= cbHexOcts*2)
			break;
		if (high_first)
			achBuf[iBuf++] = pduConvIntNibbleToChar(nLoHOct);
		else
			achBuf[iBuf++] = pduConvIntNibbleToChar(nHiHOct);
	}

	if (iBuf <= cbBuf)
	{
		achBuf[iBuf] = 0;
		return iBuf;
	}
	achBuf[cbBuf] = 0;
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int pduConvHexOctToInt(const char* HexOcts, int cbHexOcts, unsigned int* achBuf, int cbBuf)
{
	int iBuf = 0;
	unsigned char HiByte, LoByte;

	while (iBuf < cbBuf && iBuf < cbHexOcts)
	{
		HiByte = (unsigned char)*HexOcts;HexOcts++;
		LoByte = (unsigned char)*HexOcts;HexOcts++;
		achBuf[iBuf++] = (unsigned int)((HiByte << 8) | LoByte);
	}
	if (iBuf <= cbBuf)
	{
		achBuf[iBuf] = 0;
		return iBuf;
	}
	achBuf[cbBuf] = 0;
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
int pduConvStrToSemiOct(const char* HexStr, int cbHexStr, char* achOctBuf)
{
	int iBuf = 0;

	while (iBuf < cbHexStr)
	{
		// put low oct first
		achOctBuf[iBuf+1] = HexStr[iBuf];
		if (iBuf >= cbHexStr || HexStr[iBuf+1] == 0x00)
		{
			achOctBuf[iBuf] = 'F';
			break;
		}
		achOctBuf[iBuf] = HexStr[iBuf+1];
		iBuf += 2;
	}
	return (cbHexStr + cbHexStr%2);
}

///////////////////////////////////////////////////////////////////////////////
struct tm* pduCreateLinuxTime(struct timestamp_fields* pTF)
{
	struct tm* pTM = dynaCreate(sizeof(struct tm), 0);
	if (!pTM)
		goto error;

	pTM->tm_mday = pduConvSemiBCDOctToIntReverse(pTF->bDay);
	pTM->tm_hour = pduConvSemiBCDOctToIntReverse(pTF->bHour);
	pTM->tm_min = pduConvSemiBCDOctToIntReverse(pTF->bMinute);
	pTM->tm_mon = pduConvSemiBCDOctToIntReverse(pTF->bMonth);
	pTM->tm_sec = pduConvSemiBCDOctToIntReverse(pTF->bSecond);
	pTM->tm_year = pduConvSemiBCDOctToIntReverse(pTF->bYear) + 2000;

	return pTM;

error:
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduGetAddrStructLen(const struct addr_fields* pAF, addr_field_type addr_type)
{
	if (addr_type == SMSC_NUMBER)
		return pAF->cbAddrLen + sizeof(pAF->cbAddrLen);
	return (pAF->cbAddrLen + 1) / 2 + sizeof(pAF->cbAddrLen) + sizeof(pAF->bAddressType);
}
///////////////////////////////////////////////////////////////////////////////
int pduConvDcsToInt(const struct sms_dcs* dcs)
{
	return (dcs->cmd * 0x10 + dcs->param);
}
///////////////////////////////////////////////////////////////////////////////
char* pduCreateHumanAddr(const struct addr_fields* pAF, addr_field_type addr_type)
{
	int cbHumanAddr = (addr_type == SMSC_NUMBER? (pAF->cbAddrLen-1)*2:pAF->cbAddrLen) + 1;

	char* achHumanAddr = (char*)dynaCreate(cbHumanAddr, 0);

	int stat = pduConvSemiOctToStr(pAF->addrNumber, (cbHumanAddr-1), achHumanAddr, cbHumanAddr);
	if (stat < 0)
		goto error;

	return achHumanAddr;

error:
	dynaFree(achHumanAddr);
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
char* pduCreateASCIIFromGSM8Bit(const char* pGSM8Bit, int cbGSM8Bit)
{
	int cbASCII = cbGSM8Bit + 1;
	char* pASCII = dynaCreate(cbASCII, 0);

	gsm2iso(pGSM8Bit, cbGSM8Bit, pASCII, cbASCII);

	return pASCII;
}

///////////////////////////////////////////////////////////////////////////////
int pduGetGSM7BitLen(int cbStr)
{
	return (cbStr*7 + 7) / 8;
}
///////////////////////////////////////////////////////////////////////////////
char* pduCreateGSM8BitFromGSM7Bit(const char* pGSM7Bit, int cb8Bit)
{
	char* p8Bit = dynaCreate(cb8Bit, 0);

	int iGSM = 0;
	int i8Bit = 0;

	int cCarrier;
	int nRMask;
	int nCMask;

	int i7BitIdx = 0;
	int nCarrier = 0;
	while (i8Bit < cb8Bit)
	{
		cCarrier = i7BitIdx + 1;

		nRMask = (1 << (8 - cCarrier)) - 1;

		// merge carrier
		p8Bit[i8Bit++] = (pGSM7Bit[iGSM] & nRMask) << i7BitIdx | nCarrier;
		if (cb8Bit <= i8Bit)
			break;

		// add into carrier
		nCMask = (1 << cCarrier) - 1;
		nCarrier = (pGSM7Bit[iGSM] >> (8 - cCarrier)) & nCMask;

		// insert carrier only
		if (cCarrier == 7)
		{
			p8Bit[i8Bit++] = nCarrier;
			nCarrier = 0;
		}

		iGSM++;
		i7BitIdx = (i7BitIdx + 1) % 7;
	}

	if (i8Bit < cb8Bit)
		return p8Bit;

	dynaFree(p8Bit);

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
sms_encoding_type parse_msg_coding_type(struct sms_dcs *dcs)
{
	PDUP("DCS: cmd = 0x%02x, param = 0x%02x", dcs->cmd, dcs->param);
	if (dcs->cmd == 0x00 && dcs->param == 0x00)
		return DCS_7BIT_CODING;
	if (dcs->cmd < 0x08 && dcs->cmd >= 0)		/* normal sms group 1 : 00xx ~ 01xx */
	{
		/* parse bit 3, 2 */
		if ((dcs->param & 0x0c) == 0x04)		/* 8 bit data coding */
			return DCS_8BIT_CODING;
		else if ((dcs->param & 0x0c) == 0x08)	/* UCS 2 coding */
			return DCS_UCS2_CODING;
		else									/* GSM 7 bit coding */
			return DCS_7BIT_CODING;
	}
	else if ((dcs->cmd & 0x0f) == 0x0f)			/* normal sms group 2 : 1111 */
	{
		/* parse bit 2 */
		if ((dcs->param & 0x04) == 0x00)		/* GSM 7 bit coding */
			return DCS_7BIT_CODING;
		else									/* 8 bit data coding */
			return DCS_8BIT_CODING;
	}
	return DCS_7BIT_CODING;
}
///////////////////////////////////////////////////////////////////////////////
/* calculate octet or septet bytes number to skip before decoding PDU data */
static int find_msg_boundary(struct log_sms_message* pLSM, sms_encoding_type coding_type)
{
	unsigned char *cp = (unsigned char *)pLSM->pUserData;
	int skip_bytes_number = 0;

	if (pLSM->msg_type.smsDeliverType.mti == SMS_DELIVER && pLSM->msg_type.smsDeliverType.udhi == 0) {
		PDUP("SMS_DELIVER type & TP-UDHI is not set, boundary = 0");
		return 0;
	} else if (pLSM->msg_type.smsDeliverType.mti == SMS_SUBMIT && pLSM->msg_type.smsSubmitType.udhi == 0) {
		PDUP("SMS_SUBMIT type & TP-UDHI is not set, boundary = 0");
		return 0;
	}

	/* calculate total octets bits from UDHL field value */
	skip_bytes_number = (*cp + 1);
	PDUP("TP-UDHL : header length = %d octets, total header bits = %d" , *cp, skip_bytes_number*8);
	if (coding_type == DCS_8BIT_CODING || coding_type == DCS_UCS2_CODING) {
		PDUP("skip bytes(octet) = %d in coding type %d" , skip_bytes_number, coding_type);
		return skip_bytes_number;	/* octet bytes to skip */
	} else {
		/* calculate Septet boundary of GSM 7bit user data field */
		skip_bytes_number = (skip_bytes_number*8+6)/7;
		PDUP("skip bytes(septet) = %d in GSM 7 bit coding type" , skip_bytes_number);
		return skip_bytes_number;	/* septet bytes to skip */
	}
}

///////////////////////////////////////////////////////////////////////////////
static void decode_user_data_header(struct log_sms_message* pLSM, unsigned char *msg_buf)
{
	struct udh_fields udh_tmp;
	int i=0,j;
	char oct[1024];
	int tmp = 0, n, m;
	const char *color_name[] = { "Black", "Dark Grey", "Dark Red", "Dark Yellow", "Dark Green", "Dark Cyan",
						   "Dark Blue", "Dark Magenta", "Grey", "White", "Bright Red", "Bright Yellow",
						   "Bright Green", "Bright Cyan", "Bright Blue", "Bright Magenta" };
	
	(void) memset((int *)&udh_tmp, 0x0, sizeof(struct udh_fields));
	udh_tmp.udh_len = msg_buf[0];
	if (udh_tmp.udh_len == 0) {
		PDUP("UDH length is 0, give up parsing wrong UDH");
		return;
	}
	SYSLOG_DEBUG("parsing UDH field defined in 3GPP TS 123.040 9.2.3.24");
	for (i = 1; i < udh_tmp.udh_len;) {
		udh_tmp.info_id = msg_buf[i++];
		udh_tmp.info_len = msg_buf[i++];
		PDUP("information element type %d, length %d", udh_tmp.info_id, udh_tmp.info_len);

		switch (udh_tmp.info_id) {

			/* Concatenated short messages, 8-bit reference number */
			case INFO_ELEM_CONCAT_MSG_8BIT_REF:
				PDUP("UDH: < Concatenated short messages, 8-bit reference number >");
				udh_tmp.ref_no = msg_buf[i++];
				udh_tmp.msg_no = msg_buf[i++];
				udh_tmp.seq_no = msg_buf[i++];
				SYSLOG_NOTICE("UDH: msg ref no = %d, msg no = %d, seq no = %d", udh_tmp.ref_no, udh_tmp.msg_no, udh_tmp.seq_no);
				memcpy((int *)&pLSM->udh, (int *)&udh_tmp, sizeof(struct udh_fields));
				break;

			/* Special SMS Message Indication */
			case INFO_ELEM_SPECIAL_MSG_IND:
				PDUP("UDH: < Special SMS Message Indication >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Message Indication type and Storage : 0x%02x", oct[0]);
				PDUP("UDH: bit[7]: %s", (oct[0] & 0x80)? "Store message after updating indication":
													     "Discard message after updating indication");
				tmp = (oct[0] & 0x60) >> 5;
				PDUP("UDH: bit[6:5]: profile ID %d", tmp + 1);
				tmp = (oct[0] & 0x1c) >> 2;
				PDUP("UDH: bit[4:2]: %s", (tmp == 0)? "No extended message indication type":
													  "Video Message Waiting");
				tmp = oct[0] & 0x03;
				PDUP("UDH: bit[1:0]: %s", (tmp == 0)? "Voice Message Waiting":
										  (tmp == 1)? "Fax Message Waiting":
										  (tmp == 2)? "Electronic Mail Message Waiting":
											 		  "Extended Message Type Waiting");
				PDUP("UDH: oct2 : Message Count : %d", oct[1]);
				break;

			/* Application port addressing scheme, 8 bit address */
			case INFO_ELEM_APPL_PORT_ADDR_SCHEME_8BIT:
				PDUP("UDH: < Application port addressing scheme, 8 bit address >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Destination port : %d", oct[0]);
				PDUP("UDH: oct1 : Originator  port : %d", oct[1]);
				break;
				
			/* Application port addressing scheme, 16 bit address */
			case INFO_ELEM_APPL_PORT_ADDR_SCHEME_16BIT:
				PDUP("UDH: < Application port addressing scheme, 16 bit address >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1,2 : Destination port : %d", oct[0]*256+oct[1]);
				PDUP("UDH: oct3,4 : Originator  port : %d", oct[2]*256+oct[3]);
				break;

			/* SMSC Control Parameters */
			case INFO_ELEM_SMSC_CONTROL_PARAM:
				PDUP("UDH: < SMSC Control Parameters >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Selective Status Report : 0x%02x", oct[0]);
				PDUP("UDH: bit[7]: %s%s", (oct[0] & 0x80)? "Include ":"Do not include ",
										"original UDH into the Status Report");
				PDUP("UDH: bit[6]: %s%s", (oct[0] & 0x40)? "":"No ",
										"A Status Report generated, due to a error");
				PDUP("UDH: bit[3]: %s%s", (oct[0] & 0x08)? "":"No ",
										"Status Report for temp error when SC is still trying to transfer SM");
				PDUP("UDH: bit[2]: %s%s", (oct[0] & 0x04)? "":"No ",
										"Status Report for temp error when SC is not making any more transfer attempts");
				PDUP("UDH: bit[1]: %s%s", (oct[0] & 0x02)? "":"No ",
										"Status Report for permanent error when SC is not making any more transfer attempts");
				PDUP("UDH: bit[0]: %s%s", (oct[0] & 0x01)? "":"No ",
										"Status Report for short message transaction completed");
				break;

			/* UDH Source Indicator */
			case INFO_ELEM_UDH_SRC_IND:
				PDUP("UDH: < UDH Source Indicator >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : 0x%02x : %s", oct[0], 
									  (oct[0] == 1)? "The following part of the UDH is created by the original sender":
									  (oct[0] == 2)? "The following part of the UDH is created by the original receiver":
												 	 "The following part of the UDH is created by the SMSC");
				break;

			/* Concatenated short messages, 16-bit reference number */
			case INFO_ELEM_CONCAT_MSG_16BIT_REF:
				PDUP("UDH: < Concatenated short messages, 16-bit reference number >");
				udh_tmp.ref_no = msg_buf[i++]*10;
				udh_tmp.ref_no += (msg_buf[i++]);
				udh_tmp.msg_no = msg_buf[i++];
				udh_tmp.seq_no = msg_buf[i++];
				SYSLOG_NOTICE("UDH: msg ref no = %d, msg no = %d, seq no = %d", udh_tmp.ref_no, udh_tmp.msg_no, udh_tmp.seq_no);
				memcpy((int *)&pLSM->udh, (int *)&udh_tmp, sizeof(struct udh_fields));
				break;

			/* Wireless Control Message Protocol */
			case INFO_ELEM_WIRELESS_CTL_MSG_PROT:
				PDUP("UDH: < Wireless Control Message Protocol >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct length %d", udh_tmp.info_len);
				break;

			/* EMS: Text Formatting */
			case INFO_ELEM_TEXT_FORMATTING:
				PDUP("UDH: < EMS: Text Formatting >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Start position of the text formatting : 0x%02x", oct[0]);
				PDUP("UDH: oct2 : Text formatting length : 0x%02x", oct[1]);
				PDUP("UDH: oct3 : Formatting mode value : 0x%02x", oct[2]);
				PDUP("UDH: bit[7]: Style Strikethrough %s", (oct[2] & 0x80)? "on":"off");
				PDUP("UDH: bit[6]: Style Underlined %s", (oct[2] & 0x40)? "on":"off");
				PDUP("UDH: bit[5]: Style Italic %s", (oct[2] & 0x20)? "on":"off");
				PDUP("UDH: bit[4]: Style Bold %s", (oct[2] & 0x10)? "on":"off");
				tmp = (oct[2] & 0x0c) >> 2;
				PDUP("UDH: bit[3:2]: Font Size %s", (tmp == 0)? "Normal":(tmp == 1)? "Large":
												    (tmp == 2)? "Small":"Reserved");
				tmp = (oct[2] & 0x03);
				PDUP("UDH: bit[1:0]: Alignment %s", (tmp == 0)? "Left":(tmp == 1)? "Center":
													(tmp == 2)? "Right":"Language dependent");
				PDUP("UDH: oct4 : Text Colour : 0x%02x", oct[3]);
				tmp = (oct[3] & 0xf0) >> 4;
				PDUP("UDH: bit[7:4]: Text Background Colour %s", color_name[tmp]);
				tmp = (oct[3] & 0x0f);
				PDUP("UDH: bit[3:0]: Text Foreground Colour %s", color_name[tmp]);
				break;
				
			/* EMS: Predefined Sound */
			case INFO_ELEM_PREDEF_SOUND:
				PDUP("UDH: < EMS: Predefined Sound >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 : sound number : %d", oct[1]);
				break;
				
			/* EMS: User Defined Sound (iMelody max 128 bytes) */
			case INFO_ELEM_USER_DEF_SOUND:
				PDUP("UDH: < EMS: User Defined Sound (iMelody max 128 bytes) >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : User Defined Sound data", udh_tmp.info_len);
				break;
				
			/* EMS: Predefined Animation */
			case INFO_ELEM_PREDEF_ANIMATION:
				PDUP("UDH: < EMS: Predefined Animation >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 : animation number : %d", oct[1]);
				break;
				
			/* EMS: Large Animation (16*16 times 4 = 32*4 =128 bytes) */
			case INFO_ELEM_LARGE_ANIMATION:
				PDUP("UDH: < EMS: Large Animation (16*16 times 4 = 32*4 =128 bytes) >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : Large Animation data", udh_tmp.info_len);
				break;
				
			/* EMS: Small Animation (8*8 times 4 = 8*4 =32 bytes) */
			case INFO_ELEM_SMALL_ANIMATION:
				PDUP("UDH: < EMS: Small Animation (8*8 times 4 = 8*4 =32 bytes) >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : Small Animation data", udh_tmp.info_len);
				break;
				
			/* EMS: Large Picture (32*32 = 128 bytes) */
			case INFO_ELEM_LARGE_PICTURE:
				PDUP("UDH: < EMS: Large Picture (32*32 = 128 bytes) >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : Large Picture data", udh_tmp.info_len);
				break;
				
			/* EMS: Small Picture (16*16 = 32 bytes) */
			case INFO_ELEM_SMALL_PICTURE:
				PDUP("UDH: < EMS: Small Picture (16*16 = 32 bytes) >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : Small Picture data", udh_tmp.info_len);
				break;
				
			/* EMS: Variable Picture */
			case INFO_ELEM_VAR_PICTURE:
				PDUP("UDH: < EMS: Variable Picture >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 : Horizontal dimension of the picture : %d", oct[1]);
				PDUP("UDH: oct3 : Vertical dimension of the picture : %d", oct[3]);
				PDUP("UDH: oct4 - %d : Variable Picture data", udh_tmp.info_len);
				break;
				
			/* EMS: User prompt indicator */
			case INFO_ELEM_USER_PROMPT_IND:
				PDUP("UDH: < EMS: User prompt indicator >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Number of corresponding objects : %d", oct[0]);
				break;
				
			/* EMS: Extended Object */
			case INFO_ELEM_EXT_OBJ:
				PDUP("UDH: < EMS: Extended Object >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Extended Object reference number : %d", oct[0]);
				PDUP("UDH: oct2,3 : Extended Object length : %d", oct[1]*256 + oct[2]);
				PDUP("UDH: oct4 : Control data : 0x%02x", oct[3]);
				PDUP("UDH: bit[1]: Object shall be handled %s", (oct[3] & 0x02)? "as a User Prompt":"normally");
				PDUP("UDH: bit[0]: %s", (oct[3] & 0x01)? "Object shall not be forwarded by SMS":
															   "Object may be forwarded");
				PDUP("UDH: oct5 : Extended Object Type : 0x%02x", oct[4]);
				PDUP("UDH: bit[7:0]: %s", (tmp == 0)? "Predefined sound as defined in annex E.":
										  (tmp == 1)? "iMelody as defined in annex E.":
										  (tmp == 2)? "Black and white bitmap as defined in annex E.":
										  (tmp == 3)? "2-bit greyscale bitmap as defined in annex E.":
										  (tmp == 4)? "6-bit colour bitmap as defined in annex E.":
										  (tmp == 5)? "Predefined animation as defined in annex E.":
										  (tmp == 6)? "Black and white bitmap animation as defined in annex E.":
										  (tmp == 7)? "2-bit greyscale bitmap animation as defined in annex E.":
										  (tmp == 8)? "6-bit colour bitmap animation as defined in annex E.":
										  (tmp == 9)? "vCard as defined in annex E.":
										  (tmp == 10)? "vCalendar as defined in annex E.":
										  (tmp == 11)? "Standard WVG object as defined in annex E":
										  (tmp == 12)? "Polyphonic melody as defined in annex E.":
										  (tmp == 15)? "Data Format Delivery Request as defined in annex E.":
												             "Reserved"); 
				PDUP("UDH: oct6,7 : Extended Object Position : %d", oct[6]*256 + oct[7]);
				PDUP("UDH: oct8 - %d : Extended Object Data", udh_tmp.info_len);
				break;
				
			/* EMS: Reused Extended Object */
			case INFO_ELEM_REUSED_EXT_OBJ:
				PDUP("UDH: < EMS: Reused Extended Object >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Reference number of the Extended Object to be reused : %d", oct[0]);
				PDUP("UDH: oct2,3 : absolute character position : %d", oct[1]*256 + oct[2]);
				break;
				
			/* EMS: Compression Control */
			case INFO_ELEM_COMPRESSION_CONTROL:
				PDUP("UDH: < EMS: Compression Control >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Compression information : %d", oct[0]);
				tmp = (oct[0] & 0x0f);
				PDUP("UDH: bit[3:0]: Compression algorithm : %s", (tmp == 0)? "LZSS":"reserved");
				PDUP("UDH: oct2,3 : Length of the compressed data : %d", oct[1]*256 + oct[2]);
				PDUP("UDH: oct4 - %d : Compressed data", udh_tmp.info_len);
				break;
				
			/* EMS: Object Distribution Indicator */
			case INFO_ELEM_OBJ_DISTRIBUTION_IND:
				PDUP("UDH: < EMS: Object Distribution Indicator >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : Number of Information Elements : %d", oct[0]);
				PDUP("UDH: oct1 : Distribution Attributes : %d", oct[1]);
				PDUP("UDH: bit[0]: %s", (oct[1] & 0x01)? "shall not be forwarded":"may be forwarded");
				break;
				
			/* EMS: Standard WVG objectr */
			case INFO_ELEM_STD_WVG_OBJ:
				PDUP("UDH: < EMS: Standard WVG object >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : Standard WVG object bit stream", udh_tmp.info_len);
				break;
				
			/* EMS: Character Size WVG object */
			case INFO_ELEM_CHAR_SIZE_WVG_OBJ:
				PDUP("UDH: < EMS: Character Size WVG object >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : position indicating in the SM data : %d", oct[0]);
				PDUP("UDH: oct2 - %d : Character Size WVG object bit stream", udh_tmp.info_len);
				break;
				
			/* EMS: Extended Object Data Request Command */
			case INFO_ELEM_EXT_OBJ_DATA_REQ_CMD:
				PDUP("UDH: < EMS: Extended Object Data Request Command >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: no data element associated with this IE");
				break;
				
			/* RFC 822 E-Mail Header */
			case INFO_ELEM_RFC_822_EMAIL_HDR:
				PDUP("UDH: < RFC 822 E-Mail Header >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : RFC 822 E-Mail Header length indicator : %d", oct[0]);
				break;
				
			/* Hyperlink format element */
			case INFO_ELEM_HYPERLINK_FMT_ELEM:
				PDUP("UDH: < Hyperlink format element >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1,2 : Absolute Element Position : %d", oct[0]*256 + oct[1]);
				PDUP("UDH: oct3 : Hyperlink Title length : %d", oct[2]);
				PDUP("UDH: oct4 : URL length : %d", oct[3]);
				break;
				
			/* EMS: Reply Address Element */
			case INFO_ELEM_REPLY_ADDR_ELEM:
				PDUP("UDH: < EMS: Reply Address Element >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 - %d : Alternate Reply Address", udh_tmp.info_len);
				break;
				
			/* Enhanced Voice Mail Information */
			case INFO_ELEM_ENHANCED_VMAIL_INFO:
				PDUP("UDH: < Enhanced Voice Mail Information >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				/* Enhanced Voice Mail Notification */
				if ((oct[0] & 0x01) == 0) {
					PDUP("UDH: << Enhanced Voice Mail Notification >>");
					PDUP("UDH: oct1 : 0x%02x", oct[0]);
					PDUP("UDH: bit[7]: VM_MAILBOX_STATUS_EXTENSION_LENGTH parameter is %s present in this PDU",
							   (oct[0] & 0x80)? "":"not");
					PDUP("UDH: bit[6]: Voice Mailbox is %s full", (oct[0] & 0x40)? "":"not");
					PDUP("UDH: bit[5]: Voice Mailbox is %s almost full", (oct[0] & 0x20)? "":"not");
					PDUP("UDH: bit[4]: This SM shall be %s", (oct[0] & 0x10)? "stored":"discarded");
					tmp = (oct[0] & 0x0c) >> 2;
					PDUP("UDH: bit[3:2]: Multiple Subscriber Profile ID %d", tmp + 1);
					n = oct[1];
					PDUP("UDH: oct2 - %d : VM_MAILBOX_ACCESS_ADDRESS", n+2);
					PDUP("UDH: addr len %d, TOA 0x%02x", n, oct[2]);
					PDUP("UDH: oct%d : NUMBER_OF_VOICE_MESSAGES : %d", n+3, oct[n+3]);
					PDUP("UDH: oct%d : NUMBER_OF_VM_NOTIFICATIONS : %d", n+4, (oct[n+4] & 0x1f));
					PDUP("UDH: oct%d,%d : VM_MESSAGE_ID : %d", n+5, n+6, (oct[n+5]*256 + oct[n+6]));
					PDUP("UDH: oct%d : VM_MESSAGE_LENGTH : %d", n+7, oct[n+7]);
					PDUP("UDH: oct%d : VM_MESSAGE : %d", n+8, oct[n+8]);
					PDUP("UDH: bit[7] : VM_MESSAGE_EXTENSION_INDICATOR : %s", 
								(oct[n+8] & 0x80)? "Exist":"Not exist");
					PDUP("UDH: bit[6] : VM_MESSAGE_PRIORITY_INDICATION : %s", 
								(oct[n+8] & 0x40)? "Urgent":"Normal");
					PDUP("UDH: bit[4:0] : VM_MESSAGE_RETENTION_DAYS : %d", (oct[n+8] & 0x1f));
					m = oct[n+9];
					PDUP("UDH: oct%d - %d : VM_MESSAGE_CALLING_LINE_IDENTITY", n+9, n+9+m);
					PDUP("UDH: addr len %d, TOA 0x%02x", m, oct[n+9+1]);
				}
				
				/* Enhanced Voice Mail Delete Confirmation */
				else {
					PDUP("UDH: << Enhanced Voice Mail Delete Confirmation >>");
					PDUP("UDH: oct1 : 0x%02x", oct[0]);
					PDUP("UDH: bit[7]: VM_MAILBOX_STATUS_EXTENSION_LENGTH parameter is %s present in this PDU",
							   (oct[0] & 0x80)? "":"not");
					PDUP("UDH: bit[6]: Voice Mailbox is %s full", (oct[0] & 0x40)? "":"not");
					PDUP("UDH: bit[5]: Voice Mailbox is %s almost full", (oct[0] & 0x20)? "":"not");
					PDUP("UDH: bit[4]: This SM shall be %s", (oct[0] & 0x10)? "stored":"discarded");
					tmp = (oct[0] & 0x0c) >> 2;
					PDUP("UDH: bit[3:2]: Multiple Subscriber Profile ID %d", tmp + 1);
					n = oct[1];
					PDUP("UDH: oct2 - %d : VM_MAILBOX_ACCESS_ADDRESS", n+2);
					PDUP("UDH: addr len %d, TOA 0x%02x", n, oct[2]);
					PDUP("UDH: oct%d : NUMBER_OF_VOICE_MESSAGES : %d", n+3, oct[n+3]);
					PDUP("UDH: oct%d : NUMBER_OF_VM_DELETES : %d", n+4, (oct[n+4] & 0x1f));
					PDUP("UDH: oct%d,%d : VM_MESSAGE_ID : %d", n+5, n+6, (oct[n+5]*256 + oct[n+6]));
					PDUP("UDH: oct%d : VM_MESSAGE_EXTENSION : %d", n+7, oct[n+7]);
					PDUP("UDH: bit[7] : VM_MESSAGE_EXTENSION_INDICATOR : %s", 
								(oct[n+7] & 0x80)? "Exist":"Not exist");
				}
				break;
				
			/* National Language Single Shift */
			case INFO_ELEM_NAT_LANG_SGL_SFT:
				PDUP("UDH: < National Language Single Shift >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : National Language Identifier : %d", oct[0]);
				break;
				
			/* National Language Locking Shift  */
			case INFO_ELEM_NAT_LANG_LOCK_SFT:
				PDUP("UDH: < National Language Locking Shift  >");
				for(j=0; j<udh_tmp.info_len; j++) oct[j] = msg_buf[i++];
				PDUP("UDH: oct1 : National Language Locking Shift Table : %d", oct[0]);
				break;
				
			default:
				PDUP("Not supporting information element type %d", udh_tmp.info_id);
				if (udh_tmp.info_id >= 0x1B && udh_tmp.info_id <= 0x1F) {
					PDUP("Reserved for future EMS features");
				} else if (udh_tmp.info_id >= 0x26 && udh_tmp.info_id <= 0x6F) {
					PDUP("Reserved for future use");
				} else if (udh_tmp.info_id >= 0x70 && udh_tmp.info_id <= 0x7F) {
					PDUP("(U)SIM Toolkit Security Headers");
				} else if (udh_tmp.info_id >= 0x80 && udh_tmp.info_id <= 0x9F) {
					PDUP("SME to SME specific use");
				} else if (udh_tmp.info_id >= 0xA0 && udh_tmp.info_id <= 0xBF) {
					PDUP("Reserved for future use");
				} else if (udh_tmp.info_id >= 0xC0 && udh_tmp.info_id <= 0xDF) {
					PDUP("SC specific use");
				} else if (udh_tmp.info_id >= 0xE0 && udh_tmp.info_id <= 0xFF) {
					PDUP("Reserved for future use");
				}
				i += udh_tmp.info_len;
				continue;
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
static int decode_pdu_GsmBit7(struct log_sms_message* pLSM, unsigned char* msg_body, sms_encoding_type coding_type, int skip_bytes)
{
	int sidx = 0, didx = 0, bshift;
	unsigned char *msg_buf = __alloc(BSIZE_1024), *cp = (unsigned char *)pLSM->pUserData;
	unsigned char *msg_buf2 = __alloc(BSIZE_1024);
	const unsigned char *cpBody;
	unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	int cbTmpMsg = 0, cbDecodeMsg = 0;
	int octets, septets, bytes_to_decode;

	__goToErrorIfFalse(msg_buf)
	__goToErrorIfFalse(msg_buf2)
	__goToErrorIfFalse(ptmpMsg)

	septets = pLSM->cbUserData;
	octets = pduGetGSM7BitLen(septets);
	cpBody = (const unsigned char *)&msg_buf2[skip_bytes];
	bytes_to_decode = septets - skip_bytes;
	PDUP("septets = %d, octets = %d, bytes_to_decode = %d", septets, octets, bytes_to_decode);
	(void)memcpy(msg_buf, cp, octets);

	/* parse UDH field if exist */
	if (skip_bytes) {
		decode_user_data_header(pLSM, msg_buf);
	}

	/* coding 7-bits septets into octets */
	sidx = didx = 1;
	bshift = 2;
	msg_buf2[0] = msg_buf[0] & 0x7f;
	PDUP("msg_buf2[0] = %02x '%c' <-- %02x & 0x7f", msg_buf2[0], msg_buf2[0], msg_buf[0]);
	while(didx < septets) {
		msg_buf2[didx] = ((unsigned char)(msg_buf[sidx] << bshift) | (unsigned char)(msg_buf[sidx-1] >> (8-bshift))) >> 1;
		PDUP("msg_buf2[%d] = %02x '%c' <-- (%02x (= %02x << %d) | %02x (= %02x >> %d)) >> 1" ,
				didx, msg_buf2[didx], msg_buf2[didx],(unsigned char)(msg_buf[sidx] << bshift), msg_buf[sidx], bshift,
				(unsigned char)(msg_buf[sidx-1] >> (8-bshift)), msg_buf[sidx-1], (8 - bshift));
		sidx++; didx++;
		if (bshift == 7) {
			msg_buf2[didx++] = msg_buf[sidx-1] >> 1;
			PDUP("msg_buf2[%d] = %02x '%c' <-- %02x >> 1", didx-1, msg_buf2[didx-1], msg_buf2[didx-1], msg_buf[sidx-1]);
			msg_buf2[didx++] = msg_buf[sidx++] & 0x7f;
			PDUP("msg_buf2[%d] = %02x '%c' <-- %02x & 0x7f", didx-1, msg_buf2[didx-1], msg_buf2[didx-1], msg_buf[sidx-1]);
			bshift = 2;
		} else {
			bshift++;
		}
	}
	msg_buf2[didx] = ((unsigned char)(msg_buf[sidx] << bshift) | (unsigned char)(msg_buf[sidx-1] >> (8-bshift))) >> 1;

	/* decode GSM7 bits to Unicode, then decode to UTF-8 */
	cbTmpMsg = smsrecv_decodeUnicodeFromGsmBit7((char *)cpBody, ptmpMsg, bytes_to_decode);
	SYSLOG_DEBUG("decoded to Unicode, len %d, cpBody=%s", cbTmpMsg, cpBody);
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, (char *)msg_body, cbTmpMsg);
	SYSLOG_DEBUG("decoded to UTF-8, len %d, msg_body=%s", cbDecodeMsg, msg_body);
	printMsgBody((char *)msg_body, cbDecodeMsg);

	__free(msg_buf);
	__free(msg_buf2);
	__free(ptmpMsg);
	return cbDecodeMsg;
error:
	__free(msg_buf);
	__free(msg_buf2);
	__free(ptmpMsg);
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
#define SEPTET_BOUNDARY		7
static int encode_pdu_GsmBit7(unsigned char* src, int cbEncodeMsg, unsigned char* dst, int padding_bits)
{
	int didx = 0, bshift = 0, i, tmp;
	unsigned char octet;
	unsigned char *msg_buf = __alloc(BSIZE_1024);
	int octet_limit = pduGetGSM7BitLen(cbEncodeMsg);
	__goToErrorIfFalse(msg_buf)
	
	/* add padding bits first */
   if(padding_bits) {
      bshift = SEPTET_BOUNDARY - padding_bits;
      msg_buf[didx++] = src[0] << (SEPTET_BOUNDARY - bshift);
      bshift++;
   }
   for(i = 0; i < cbEncodeMsg; i++) {
      if(bshift == SEPTET_BOUNDARY) {
         bshift = 0;
         continue;
      }
      if(didx > octet_limit)
         return 0; /* buffer overflow */
      octet = (src[i] & 0x7f) >> bshift;
      if( i < cbEncodeMsg - 1 )
         octet |= src[i + 1] << (SEPTET_BOUNDARY - bshift);
      msg_buf[didx++] = octet;
      bshift++;
   }
	for (i = 0; i < pduGetGSM7BitLen(cbEncodeMsg); i++) {
		tmp = msg_buf[i];
		dst[i*2] = (unsigned char)pduConvIntNibbleToChar(((tmp & 0xf0) >> 4));
		dst[i*2+1] = (unsigned char)pduConvIntNibbleToChar((tmp & 0x0f));
		PDUP("dst[%d] = %02x, %c", i*2, dst[i*2], dst[i*2]);
		PDUP("dst[%d] = %02x, %c", i*2+1, dst[i*2+1], dst[i*2+1]);
	}
	__free(msg_buf);
	return (didx);
error:
	__free(msg_buf);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int decode_pdu_GsmBit8(struct log_sms_message* pLSM, unsigned char* msg_body, sms_encoding_type coding_type, int skip_bytes)
{
	unsigned char *cpBody = (unsigned char *)pLSM->pUserData;
	int msg_len = 0, cbDecodeMsg = 0;

	int cbTmpMsg; 
	unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	__goToErrorIfFalse(ptmpMsg)
	
	/* parse UDH field if exist */
	if (skip_bytes) {
		decode_user_data_header(pLSM, cpBody);
	}
	cpBody += skip_bytes;
	
	/* GSM 8 bit or binary can not be parsed. Just copy it. */
	msg_len = pLSM->cbUserData - skip_bytes;
	cbDecodeMsg = msg_len*2;

	/* decode GSM7 bits to Unicode, then decode to UTF-8 */
	cbTmpMsg = smsrecv_decodeUnicodeFromGsmBit7((char *)cpBody, ptmpMsg, msg_len);
	SYSLOG_DEBUG("decoded to Unicode, len %d, cpBody=%s", cbTmpMsg, cpBody);
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, (char *)msg_body, cbTmpMsg);
	SYSLOG_DEBUG("decoded to UTF-8, len %d, msg_body=%s", cbDecodeMsg, msg_body);
	printMsgBody((char *)msg_body, cbDecodeMsg);

#if (0)
	if (pduConvHexOctToStr((const char*)cpBody, msg_len, (char *)msg_body, cbDecodeMsg , TRUE) < 0) {
		SYSLOG_ERR("8 BIT format conversion error");
		return 0;
	}
#endif	
	SYSLOG_DEBUG("bypass decoding for GSM 8 bit or binary format, len %d", cbDecodeMsg);
	return cbDecodeMsg;

error:
	__free(ptmpMsg);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int decode_pdu_CdmaBit8(struct log_sms_message* pLSM, unsigned char* msg_body, sms_encoding_type coding_type, int skip_bytes)
{
	unsigned char *cpBody = (unsigned char *)pLSM->pUserData;
	int msg_len = 0, cbDecodeMsg = 0;

	int cbTmpMsg;
	unsigned int* ptmpMsg = __alloc(MAX_UTF8_BUF_SIZE);
	__goToErrorIfFalse(ptmpMsg)

	/* parse UDH field if exist */
	if (skip_bytes) {
		decode_user_data_header(pLSM, cpBody);
	}
	cpBody += skip_bytes;

	/* GSM 8 bit or binary can not be parsed. Just copy it. */
	msg_len = pLSM->cbUserData - skip_bytes;
	cbDecodeMsg = msg_len*2;

	cbTmpMsg = smsrecv_decodeUnicodeFromASCII((char *)cpBody, ptmpMsg, msg_len);
	SYSLOG_DEBUG("decoded to Unicode, len %d, cpBody=%s", cbTmpMsg, cpBody);
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, (char *)msg_body, cbTmpMsg);
	SYSLOG_DEBUG("decoded to UTF-8, len %d, msg_body=%s", cbDecodeMsg, msg_body);
	printMsgBody((char *)msg_body, cbDecodeMsg);

	SYSLOG_DEBUG("bypass decoding for GSM 8 bit or binary format, len %d", cbDecodeMsg);
	return cbDecodeMsg;

error:
	__free(ptmpMsg);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
static int smsrecv_decodeUcs2(unsigned char* cpBody, unsigned int* pDecodeMsg, int cbBody)
{
	return pduConvHexOctToInt((const char *)cpBody, cbBody*2, pDecodeMsg, cbBody/2);
}
///////////////////////////////////////////////////////////////////////////////
/* msg_body : MAX_UNICODE_BUF_SIZE = 1024 * 4 */
static int decode_pdu_Ucs2(struct log_sms_message* pLSM, unsigned char* msg_body, sms_encoding_type coding_type, int skip_bytes)
{
	unsigned char *cpBody = (unsigned char *)pLSM->pUserData;
	unsigned int* ptmpMsg = __alloc(MAX_UNICODE_BUF_SIZE);
	int cbTmpMsg = 0;
	int cbDecodeMsg = 0;

	__goToErrorIfFalse(ptmpMsg)

	/* parse UDH field if exist */
	if (skip_bytes) {
		decode_user_data_header(pLSM, cpBody);
	}
	
	cpBody += skip_bytes;
	/* decode PDU to Unicode, then decode to UTF-8 */
	cbTmpMsg = smsrecv_decodeUcs2(cpBody, ptmpMsg, pLSM->cbUserData-skip_bytes);
	SYSLOG_DEBUG("decoded to Unicode, len %d", cbTmpMsg);
	if (cbTmpMsg <= 0) {
		SYSLOG_ERR("Unicode decoding error");
		__goToError()
	}
	printMsgBodyInt((int *)ptmpMsg, cbTmpMsg);
	cbDecodeMsg = smsrecv_decodeUtf8FromUnicode(ptmpMsg, (char *)msg_body, cbTmpMsg);
	SYSLOG_DEBUG("decoded to UTF-8, len %d", cbDecodeMsg);
	printMsgBody((char *)msg_body, cbDecodeMsg);
	return cbDecodeMsg;
error:
	__free(ptmpMsg);
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
/* msg_body : MAX_UNICODE_BUF_SIZE = 1024 * 4 */
int decode_pdu_msg_body(struct log_sms_message* pLSM, unsigned char* msg_body, sms_encoding_type coding_type)
{
	int skip_bytes;
	(void) memset(msg_body, 0x00, MAX_UNICODE_BUF_SIZE);
	skip_bytes = find_msg_boundary(pLSM, coding_type);
	if (coding_type == DCS_7BIT_CODING)
		return decode_pdu_GsmBit7(pLSM, msg_body, coding_type, skip_bytes);
	else if (coding_type == DCS_8BIT_CODING) {
		if (!is_cinterion_cdma)
			return decode_pdu_GsmBit8(pLSM, msg_body, coding_type, skip_bytes);
		else
			return decode_pdu_CdmaBit8(pLSM, msg_body, coding_type, skip_bytes);
	}
	else if (coding_type == DCS_UCS2_CODING)
		return decode_pdu_Ucs2(pLSM, msg_body, coding_type, skip_bytes);
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
void pduFreeLogSMSMessage(void* pRef)
{
	struct log_sms_message* pLSM = (struct log_sms_message*)pRef;

	dynaFree(pLSM->pRawSMSMessage);

	pLSM->pRawSMSMessage = 0;
}
///////////////////////////////////////////////////////////////////////////////
int last_used_tpmr = -1;
struct log_sms_message* pduCreateLogSMSMessage(const char* szStrSMSMessage, tpmti_field_type msg_type)
{
	// get
	struct log_sms_message* pLSM = dynaCreate(sizeof(struct log_sms_message), pduFreeLogSMSMessage);
	sms_encoding_type coding_type;
	unsigned char *pOct;
	char *tmp_str;
	struct tm* ts = 0;
	int mismatching_gap;
	if (!pLSM)
		goto error;

	// get raw SMS message
	pLSM->pRawSMSMessage = pduCreateOctByStr(szStrSMSMessage, (int *)&pLSM->cbRawSMSMessage);
	if (!pLSM->pRawSMSMessage)
		goto error;

	pOct = (unsigned char *)pLSM->pRawSMSMessage;
	PDUP("------ PDU Message Parsing ------");

	// get SMSC address
	pLSM->pSMSCNumber = (const struct addr_fields*)pOct;
	PDUP("PDU SMSC: len %d", pLSM->pSMSCNumber->cbAddrLen);
	PDUP("PDU SMSC: type %d", pLSM->pSMSCNumber->bAddressType);
	tmp_str = pduCreateHumanAddr(pLSM->pSMSCNumber, SMSC_NUMBER);
	PDUP("PDU SMSC: addr %s", tmp_str);
	pOct += pduGetAddrStructLen(pLSM->pSMSCNumber, SMSC_NUMBER);

	// get TP type
	pLSM->msg_type.smsDeliverType = *(struct sms_deliver_type*)pOct;
	PDUP("PDU HEADER: 0x%02x", *pOct);
	pOct += sizeof(struct sms_deliver_type);

	/* some modules respond with wrong message type(Ericsson etc,.) so do not use
	* parsed message type.
	*/
	if (pLSM->msg_type.smsDeliverType.mti != msg_type) {
		SYSLOG_ERR("mti mismatch: parse %d, expect %d", pLSM->msg_type.smsDeliverType.mti, msg_type);
		pLSM->msg_type.smsDeliverType.mti = msg_type;
	}

	if (pLSM->msg_type.smsDeliverType.mti == SMS_DELIVER ||
		pLSM->msg_type.smsDeliverType.mti == SMS_RESERVED_TYPE) {
		PDUP("PDU HEADER: SMS_DELIVER");
		PDUP("   TP-MTI : %d", pLSM->msg_type.smsDeliverType.mti);
		PDUP("   TP-MMS : %d", pLSM->msg_type.smsDeliverType.mms);
		PDUP("   TP-LP  : %d", pLSM->msg_type.smsDeliverType.lp);
		PDUP("   TP-SRI : %d", pLSM->msg_type.smsDeliverType.sri);
		PDUP("   TP-UDHI: %d", pLSM->msg_type.smsDeliverType.udhi);
		PDUP("   TP-RP  : %d", pLSM->msg_type.smsDeliverType.rp);
		// get originating address
		pLSM->addr.pOrigAddr = (struct addr_fields*)pOct;
		pOct += pduGetAddrStructLen(pLSM->addr.pOrigAddr, MOBILE_NUMBER);
	} else if (pLSM->msg_type.smsDeliverType.mti == SMS_SUBMIT) {
		PDUP("PDU HEADER: SMS_SUBMIT");
		PDUP("   TP-MTI : %d", pLSM->msg_type.smsSubmitType.mti);
		PDUP("   TP-RD  : %d", pLSM->msg_type.smsSubmitType.rp);
		PDUP("   TP-VPF : %d", pLSM->msg_type.smsSubmitType.vpf);
		PDUP("   TP-SRR : %d", pLSM->msg_type.smsSubmitType.srr);
		PDUP("   TP-UDHI: %d", pLSM->msg_type.smsSubmitType.udhi);
		PDUP("   TP-RP  : %d", pLSM->msg_type.smsSubmitType.rp);
		// get message reference
		pLSM->SubmitMsgRef = *pOct;
		if (pLSM->SubmitMsgRef > last_used_tpmr)
			last_used_tpmr = pLSM->SubmitMsgRef;
		pOct++;
		// get destination address
		pLSM->addr.pDestAddr = (struct addr_fields*)pOct;
		pOct += pduGetAddrStructLen(pLSM->addr.pDestAddr, MOBILE_NUMBER);
	} else {
		SYSLOG_ERR("Not suppoted message type %d", pLSM->msg_type.smsDeliverType.mti);
		goto error;
	}

	pLSM->dwPID = *pOct++;
	PDUP("PDU TP-PID: 0x%02x", pLSM->dwPID);

	// dcs
	pLSM->dcs = *(struct sms_dcs*)pOct;
	PDUP("PDU TP-DCS: 0x%02x", *pOct);
	pOct += sizeof(struct sms_dcs);

	if (pLSM->msg_type.smsDeliverType.mti == SMS_SUBMIT) {
		// get validity period
		if (pLSM->msg_type.smsSubmitType.vpf == RELATIVE_VPF_FORMAT) {
			pLSM->SubmitValidPeriod.rel_tpvp = *pOct;
			PDUP("PDU REL-TPVP: 0x%04x", *pOct);
			pOct++;
		} else if (pLSM->msg_type.smsSubmitType.vpf == ABSOLUTE_VPF_FORMAT) {
			pLSM->SubmitValidPeriod.abs_tpvp = (struct timestamp_fields*)pOct;
			PDUP("PDU ABS-TPVP: %02x%02x%02x%02x%02x%02x%02x", *pOct, *(pOct+1), *(pOct+2), *(pOct+3), *(pOct+4), *(pOct+5), *(pOct+6));
			pOct += sizeof(struct timestamp_fields);
		} else if (pLSM->msg_type.smsSubmitType.vpf == ENHANCED_VPF_FORMAT) {
			pLSM->SubmitValidPeriod.enh_tpvp = (struct enhanced_tpvp_fields*)pOct;
			PDUP("PDU ENH-TPVP: %02x%02x%02x%02x%02x%02x%02x", *pOct, *(pOct+1), *(pOct+2), *(pOct+3), *(pOct+4), *(pOct+5), *(pOct+6));
			pOct += sizeof(struct timestamp_fields);
		}
	} else {
		// get timestamp
		pLSM->pTimeStamp = (struct timestamp_fields*)pOct;
		ts = pduCreateLinuxTime(pLSM->pTimeStamp);
		PDUP("PDU TP-SCTS: %04d/%02d/%02d,%02d:%02d:%02d",
					ts->tm_year, ts->tm_mon, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
		pOct += sizeof(*pLSM->pTimeStamp);
	}

	// get user data
	//pLSM->cbUserData = (unsigned int)(*pOct);
	pLSM->cbUserData = (*pOct);
	SYSLOG_DEBUG("pLSM->cbUserData = %d, *pOct = 0x%x", pLSM->cbUserData, *pOct);
	PDUP("PDU TP-UDL: 0x%x (%d)", *pOct, *pOct);
	pOct++;
	pLSM->pUserData = pOct;

	coding_type = parse_msg_coding_type((struct sms_dcs *)&pLSM->dcs);
	if (coding_type == DCS_7BIT_CODING)
		pOct += pduGetGSM7BitLen(pLSM->cbUserData);
	else
		pOct += pLSM->cbUserData;

	int cbTotal = pOct - (unsigned char*)pLSM->pRawSMSMessage;
	if (cbTotal != pLSM->cbRawSMSMessage-1) {
		mismatching_gap = cbTotal - pLSM->cbRawSMSMessage + 1;
		SYSLOG_ERR("msg len mismatch: cbTotal %d, Calc %d, gap %d", cbTotal, pLSM->cbRawSMSMessage-1, mismatching_gap);
		/* recalculate actual user data length */
		if (coding_type == DCS_7BIT_CODING) {
			pLSM->cbUserData -= (mismatching_gap*8/7+2);
		} else {
			pLSM->cbUserData -= (mismatching_gap+1);
		}
		SYSLOG_ERR("recalculated user data length = %d", pLSM->cbUserData);
	}

	return pLSM;

error:
	dynaFree(pLSM);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int createPduData(char *destno, int int_no, int ucs_coding, unsigned char *pEncodeMsg, int cbEncodeMsg,
		unsigned char *pPduData, struct addr_fields *smsc_addr, int udh_len)
{
	struct log_sms_message* pLSM = dynaCreate(sizeof(struct log_sms_message), pduFreeLogSMSMessage);
	unsigned char* pPdu = pPduData;
	unsigned char* pTpduBegin;
	struct addr_fields* pDestAddr = dynaCreate(sizeof(struct addr_fields)+MAX_ADDR_LEN, 0);
	int hexStrLen;
	int encoded_msg_len = 0, padding_bits = 0;

	if (!pLSM || !pDestAddr)
		goto error;

	/* fill basic submit message container */
	pLSM->msg_type.smsSubmitType.mti = SMS_SUBMIT;
	pLSM->msg_type.smsSubmitType.rd = 0;
	pLSM->msg_type.smsSubmitType.vpf = RELATIVE_VPF_FORMAT;
	pLSM->msg_type.smsSubmitType.srr = 0;
	pLSM->msg_type.smsSubmitType.udhi = udh_len? 1:0;
	pLSM->msg_type.smsSubmitType.rp = 0;
	pLSM->SubmitMsgRef = ++last_used_tpmr;
	pLSM->addr.pDestAddr = pDestAddr;
	pLSM->addr.pDestAddr->cbAddrLen = (unsigned char)strlen(destno);
	(void) strcpy((char *)&pLSM->addr.pDestAddr->addrNumber, destno);
	/* 0xA1 should be right TOA according to 3GPP specification but NTT DoCoMo accepts 0x81 instead of that. */
	if (!strncmp(model_variants_name(), "PHS8-J", 6)) {
		pLSM->addr.pDestAddr->bAddressType = 0x80 | (int_no? atInternational:0) << 4 | npiIsdnTelephoneNumberingPlan;
	} else {
		pLSM->addr.pDestAddr->bAddressType = 0x80 | (int_no? atInternational:atNational) << 4 | npiIsdnTelephoneNumberingPlan;
	}
	pLSM->dwPID = 0;
	pLSM->dcs.cmd = (ucs_coding? 8:0);
	pLSM->dcs.param = 0;
	/* workaround for NTT DoCoMo Japan that does not accept VP longer than 1 day */
	//pLSM->SubmitValidPeriod.rel_tpvp = 0xAA;		/* default vp : (170 - 166) * 1 day */
	pLSM->SubmitValidPeriod.rel_tpvp = 0xA7;		/* default vp : (167 - 166) * 1 day */
	/* test encode message body to determine encoded message body size */
	if (ucs_coding) {
		encoded_msg_len = (strlen((char *)pEncodeMsg)/2);
	} else {
		/* UDL = septets number in GSM7 mode */
		//encoded_msg_len = cbEncodeMsg;
		if (udh_len)
			//encoded_msg_len = 6+((cbEncodeMsg-6)*7+7)/8;
			encoded_msg_len = (cbEncodeMsg-12)+udh_len/2+1;		/* +1 for loss of padding bits */
		else
			encoded_msg_len = cbEncodeMsg;
	}
	pLSM->cbUserData = encoded_msg_len;
	SYSLOG_DEBUG("PDU : pLSM->cbUserData = %d", pLSM->cbUserData);
	/* convert to pdu format */
	hexStrLen = smsc_addr->cbAddrLen;
	smsc_addr->cbAddrLen = (smsc_addr->cbAddrLen+1)/2 +1;	/* octet number */
	pPdu += pduConvHexOctToStr((const char *)&smsc_addr->cbAddrLen, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvHexOctToStr((const char *)&smsc_addr->bAddressType, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvStrToSemiOct((const char *)&smsc_addr->addrNumber, hexStrLen, (char *)pPdu);
	pTpduBegin = pPdu;
	pPdu += pduConvHexOctToStr((const char *)&pLSM->msg_type.smsSubmitType, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvHexOctToStr((const char *)&pLSM->SubmitMsgRef, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvHexOctToStr((const char *)&pLSM->addr.pDestAddr->cbAddrLen, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvHexOctToStr((const char *)&pLSM->addr.pDestAddr->bAddressType, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvStrToSemiOct((const char *)&pLSM->addr.pDestAddr->addrNumber, pLSM->addr.pDestAddr->cbAddrLen, (char *)pPdu);
	pPdu += pduConvHexOctToStr((const char *)&pLSM->dwPID, 1, (char *)pPdu, 2, TRUE);
	pPdu += pduConvHexOctToStr((const char *)&pLSM->dcs, 1, (char *)pPdu, 2, FALSE);
	pPdu += pduConvHexOctToStr((const char *)&pLSM->SubmitValidPeriod.rel_tpvp, 1, (char *)pPdu, 2, TRUE);
	/* convert to pdu format */
	pPdu += pduConvHexOctToStr((const char *)&pLSM->cbUserData, 1, (char *)pPdu, 2, TRUE);
	if (ucs_coding) {
		strcpy((char *)pPdu, (char *)pEncodeMsg);
		encoded_msg_len = (strlen((char *)pEncodeMsg)/2);
	} else {
		if (udh_len) {
			strncpy((char *)pPdu, (char *)pEncodeMsg, udh_len);
			padding_bits = 7-((udh_len/2)*8)%7;		/* pad bits for septet boundary of user data header */
			(void)encode_pdu_GsmBit7(pEncodeMsg+udh_len, cbEncodeMsg-udh_len, pPdu+udh_len, padding_bits);
		} else {
			(void)encode_pdu_GsmBit7(pEncodeMsg, cbEncodeMsg, pPdu, 0);
		}
	}
	dynaFree(pDestAddr);
	dynaFree(pLSM);
	return (strlen((char *)pTpduBegin)/2);

error:
	dynaFree(pDestAddr);
	dynaFree(pLSM);
	return 0;
}


/* ---------------------------------------------*/
/*			CDMA			*/
/* ---------------------------------------------*/

///////////////////////////////////////////////////////////////////////////////
int pduGetCDMAPduData(uint8_t *src, uint8_t * dest, int size)
{
	if (src == NULL || dest == NULL || size <= 0)
	{
		return 0;
	}

	memcpy(dest, src, size);
	return size;
}
///////////////////////////////////////////////////////////////////////////////
// Return Value: success - 0
//               failure - -1
int getBitsData (uint32_t *dest, uint8_t * src, int bytes_of_src, int start_pos, int bit_len)
{
	int s_index = 0, e_index = 0;
	int s_bPos = 0, e_bPos = 0;
	int bSizeOfsrc = bytes_of_src * 0x08;
	uint8_t shift = 0x00, mask = 0x00, dest_shift = 0x00;

	if (dest == NULL || src == NULL || bytes_of_src <= 0 || start_pos < 0 || bit_len <= 0)
	{
		SYSLOG_ERR("CDMA SMS: getBitData: Invalid Input");
		return -1;
	}

	if ( bSizeOfsrc < start_pos + bit_len )
	{
		SYSLOG_ERR("CDMA SMS: Out of Range");
		return -1;
	}

	if ( bit_len > 0x08*sizeof(uint32_t))
	{
		SYSLOG_ERR("CDMA SMS: Dest Buffer is NOT enough");
		return -1;
	}

	s_index = start_pos/8;
	s_bPos = start_pos%8;
	e_index = (start_pos + bit_len)/8;
	e_bPos = (start_pos + bit_len)%8;

	*dest = 0;

	for ( ; s_index <= e_index; s_index++ )
	{
		if (s_index < e_index)
		{
			shift = 0x00;
			dest_shift = 0x08 - s_bPos;
			mask  = (uint8_t) ((0x01 << dest_shift) - 0x01);
			s_bPos = 0;
		}
		else
		{
			shift = (uint8_t) (8 - e_bPos);
			dest_shift = e_bPos - s_bPos;
			mask  = (uint8_t) ((0x01 << dest_shift) - 0x01);
		}

		if ( mask != 0x00)
			*dest = (*dest << dest_shift) | ((src[s_index] >> shift) & mask);

	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
// This function is to set last "set_bits" bits of src to dest starting from sPos_dest bit offset
//
//	dest - Start Address of destination buffer.
//	bytes_of_dest - the number of bytes of dest buffer. MAX: 0x01FFFFFF
//	sPos_dest - start bit offset of dest to be written 
//	src - data to be written
//	set_bits - the number of bits to be written. Last "set_bits" bits of src is written.
//
// Return Value: success - 0
//               failure - -1
int setBitsData (uint8_t *dest, int bytes_of_dest, unsigned int * sPos_dest, uint32_t src, unsigned int set_bits)
{
	int s_ArrIdx = 0;
	int s_bPos = 0, e_bPos = 0;
	int totalLength = 0;
	int bSizeOfSrc = 8 * sizeof(src);
	uint8_t mask = 0x00, setVal=0x00;
	int src_shift = 0;

	if (dest == NULL || sPos_dest == NULL) {
		SYSLOG_ERR("CDMA SMS: Invalid Input");
		return -1;
	}

	if (bytes_of_dest < 0 || bytes_of_dest > 0x01FFFFFF) {
		SYSLOG_ERR("CDMA SMS: dest buffer is too big");
		return -1;
	}

	if (bytes_of_dest != 0 && (((*sPos_dest+set_bits) > bytes_of_dest*8) || ( bSizeOfSrc < set_bits))) {
		SYSLOG_ERR("CDMA SMS: Out of range");
		return -1;
	}

	s_ArrIdx = *sPos_dest/8;
	s_bPos = *sPos_dest%8;
	e_bPos = (*sPos_dest + set_bits)%8;
	totalLength = set_bits + s_bPos;

	//SYSLOG_ERR("totalLength=[%d,]s_ArrIdx=[%d], s_bPos=[%d], e_bPos=[%d]", totalLength, s_ArrIdx, s_bPos, e_bPos);

	if(totalLength < 8)
	{
		mask = (0xFF >> s_bPos) & (0xFF << (8 - e_bPos));
		src_shift = 8- totalLength;
		setVal = (src << src_shift) & mask;
		
		//SYSLOG_ERR("mask=[0x%02x], src_shift=[%d], setVal=[0x%02x]", mask, src_shift, setVal);
		
		dest[s_ArrIdx] = (dest[s_ArrIdx] & ~mask) | setVal;
		*sPos_dest += set_bits;
		return 0;
	}

	if (s_bPos != 0) {
		mask = (0xFF >> s_bPos);
		src_shift = 8- totalLength;
		
		if (src_shift >= 0)
			setVal = (src << src_shift) & mask;
		else
			setVal = (src >> -1*src_shift) & mask;
		
		//SYSLOG_ERR("mask=[0x%02x], src_shift=[%d], setVal=[0x%02x]", mask, src_shift, setVal);
		
		dest[s_ArrIdx] = (dest[s_ArrIdx] & ~mask) | setVal;
		s_ArrIdx++;
		totalLength -= 8;
	}

	for (; totalLength >= 8; )
	{
		mask = 0xFF;
		src_shift = 8- totalLength;

		if (src_shift >= 0)
			setVal = (src << src_shift) & mask;
		else
			setVal = (src >> -1*src_shift) & mask;

		//SYSLOG_ERR("mask=[0x%02x], src_shift=[%d], setVal=[0x%02x]", mask, src_shift, setVal);
		
		dest[s_ArrIdx] = (dest[s_ArrIdx] & ~mask) | setVal;
		s_ArrIdx++;
		totalLength -= 8;
	}

	if (totalLength > 0)
	{
		mask = (0xFF << (8 - e_bPos));
		src_shift = 8- totalLength;

		if (src_shift >= 0)
			setVal = (src << src_shift) & mask;
		else
			setVal = (src >> -1*src_shift) & mask;
		
		//SYSLOG_ERR("mask=[0x%02x], src_shift=[%d], setVal=[0x%02x]", mask, src_shift, setVal);
		
		dest[s_ArrIdx] = (dest[s_ArrIdx] & ~mask) | setVal;
	}
	*sPos_dest += set_bits;
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduCDMAConvVoidOctToStr(unsigned char * dest, void *src, int bytesOfsrc)
{
	#define VOIDBUF_SIZE	32
	uint32_t iVal;
	uint8_t buf[VOIDBUF_SIZE];
	int len = 0;
	unsigned int bPos=0;
	if (dest == NULL || src == NULL || bytesOfsrc <= 0)
		goto error;

	memset(buf, 0, VOIDBUF_SIZE);
	iVal = *(uint32_t*)src;

	PDUP("CDMA SMS: pduCDMAConvVoidOctToStr bytesOfsrc=[%d], iVal=[%d]", bytesOfsrc, iVal);

	if(setBitsData(buf, VOIDBUF_SIZE, &bPos, iVal, bytesOfsrc*8))
		goto error;

	len =  pduConvHexOctToStr((const char *)&buf, 2*bytesOfsrc,(char *)  dest, 2*2*bytesOfsrc, TRUE);
	if (len < 0)
		goto error;

	return len;
error:
	SYSLOG_ERR("CDMA SMS: ERROR on pduCDMAConvVoidOctToStr");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduCDMAConvDestAddrToStr(unsigned char * dest, cdma_sms_address_s_type * addr)
{
	#define ADDRBUF_SIZE	128
	int len =0, cnt = 0;
	uint8_t param_len = 0x0, param_id = 0x04;
	uint8_t bBuf[ADDRBUF_SIZE];
	unsigned char * pPdu = dest;
	uint8_t dtmf = 0;
	unsigned int bPos=0;

	if (dest == NULL || addr == NULL)
		goto error;

	memset(bBuf, 0, ADDRBUF_SIZE);

	if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, addr->digit_mode, 1))
		goto error;

	if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, addr->number_mode, 1))
		goto error;

	if(addr->digit_mode == CDMA_SMS_DIGIT_MODE_8_BIT)
	{
		if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, addr->number_type, 3))
			goto error;

		if(addr->number_mode == CDMA_SMS_NUMBER_MODE_NONE_DATA_NETWORK) {
			if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, addr->number_plan, 4))
				goto error;
		}
	}

	if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, addr->num_fields, 8))
		goto error;

	if(addr->digit_mode == CDMA_SMS_DIGIT_MODE_8_BIT)
	{
		for (cnt = 0; cnt < addr->num_fields && cnt < MAX_ADDR_LEN; cnt++)
		{
			if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, addr->chari[cnt], 8))
				goto error;
		}
	}
	else
	{
		for (cnt = 0; cnt < addr->num_fields && cnt < MAX_ADDR_LEN; cnt++)
		{
			switch (addr->chari[cnt])
			{
				case '0':
					dtmf = 0x0A;
					break;
				case '*':
					dtmf = 0x0B;
					break;
				case '#':
					dtmf = 0x0C;
					break;
				default:
					dtmf = addr->chari[cnt] -'0';
					break;
			}

			if(0 > setBitsData(bBuf, ADDRBUF_SIZE, &bPos, dtmf, 4))
				goto error;
		}
	}

	if (bPos == 0)
		goto error;

	param_len =(uint8_t) bPos/8;
	if (bPos%8 != 0)
		param_len++;

	len += pduConvHexOctToStr((const char*)&param_id, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&param_len, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&bBuf, 2*param_len, (char *) pPdu+len, 4*param_len, TRUE);
	return len;
error:
	SYSLOG_ERR("CDMA SMS: pduCDMAConvDestAddrToStr: Return Error");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduCDMAConvBearerDataToStr(unsigned char * dest, cdma_sms_bearer_data_s_type * bData)
{
	#define USERDATABUF_SIZE	1024
	int len =0, cnt = 0, bytesOfchari = 0, bitsOfchari = 0;
	uint8_t param_len = 0x0, param_id = 0x08;
	uint8_t subparam_len = 0x0, subparam_id = 0x0;
	uint8_t ud_subparam_len = 0x0;
	uint8_t miBuf[8];
	uint8_t udBuf[USERDATABUF_SIZE];
	unsigned char * pPdu = dest;
	unsigned int mi_bPos=0, ud_bPos=0;

	if (dest == NULL || bData == NULL)
		goto error;

	memset(miBuf, 0, 8);
	memset(udBuf, 0, USERDATABUF_SIZE);

	//Set Message Identifier SubParameter
	if(0 > setBitsData(miBuf, 8, &mi_bPos, bData->bd_msg_id.bd_sub_msg_type, 4))
		goto error;

	if(0 > setBitsData(miBuf, 8, &mi_bPos, bData->bd_msg_id.bd_sub_msg_id, 16))
		goto error;

	if(0 > setBitsData(miBuf, 8, &mi_bPos, bData->bd_msg_id.bd_sub_header_ind, 1))
		goto error;

	if(0 > setBitsData(miBuf, 8, &mi_bPos, 0, 3)) // 3 RESERVED bits
		goto error;

	//Set User Data SubParameter
	if(0 > setBitsData(udBuf, USERDATABUF_SIZE, &ud_bPos, bData->bd_user_data.bd_sub_msg_enc, 5))
		goto error;

	if(bData->bd_user_data.bd_sub_msg_enc == CDMA_BD_SUB_MSG_ENC_EXT_PM
		|| bData->bd_user_data.bd_sub_msg_enc == CDMA_BD_SUB_MSG_ENC_EXT_GSM)
	{
		if(0 > setBitsData(udBuf, USERDATABUF_SIZE, &ud_bPos, bData->bd_user_data.bd_sub_msg_type, 8))
			goto error;
	}

	if(0 > setBitsData(udBuf, USERDATABUF_SIZE, &ud_bPos, bData->bd_user_data.bd_sub_num_fields, 8))
		goto error;

	switch (bData->bd_user_data.bd_sub_msg_enc)
	{
		case CDMA_BD_SUB_MSG_ENC_7BIT_ACSII:
		case CDMA_BD_SUB_MSG_ENC_IA5:
		case CDMA_BD_SUB_MSG_ENC_7BIT_GSM:
			bitsOfchari = 7;
			bytesOfchari = bData->bd_user_data.bd_sub_num_fields;
			break;

		case CDMA_BD_SUB_MSG_ENC_8BIT_OCT:
		case CDMA_BD_SUB_MSG_ENC_LATIN_HEB:
		case CDMA_BD_SUB_MSG_ENC_LATIN:
			bitsOfchari = 8;
		 	bytesOfchari = bData->bd_user_data.bd_sub_num_fields;
			break;

		case CDMA_BD_SUB_MSG_ENC_UNICODE: //16-bit
			bitsOfchari = 8;
			bytesOfchari = 2 * bData->bd_user_data.bd_sub_num_fields;
			break;

		case CDMA_BD_SUB_MSG_ENC_JIS:
		case CDMA_BD_SUB_MSG_ENC_KOREAN:
			bitsOfchari = 8;
			bytesOfchari = bData->bd_user_data.bd_sub_num_fields;
			break;

		case CDMA_BD_SUB_MSG_ENC_EXT_GSM:
		case CDMA_BD_SUB_MSG_ENC_EXT_PM:
		default:
			SYSLOG_ERR("CDMA SMS: not supportted encoding type [%d]", bData->bd_user_data.bd_sub_msg_enc);
			goto error;
			break;
	}

	PDUP("CDMA SMS: pduCDMAConvBearerDataToStr bitsOfchari=[%d], bytesOfchari=[%d]", bitsOfchari, bytesOfchari);
	PDUP("CDMA SMS: pduCDMAConvBearerDataToStr Msg Body=[%s]", bData->bd_user_data.log_chari);

	if(bData->bd_user_data.bd_sub_msg_enc == CDMA_BD_SUB_MSG_ENC_UNICODE) {
		int lenOfpOct=0;
		unsigned char * pOct = (unsigned char *) pduCreateOctByStr((const char*) bData->bd_user_data.log_chari, (int *)&lenOfpOct);
		for(cnt=0 ; cnt < lenOfpOct ; cnt++)
		{
			if(0 > setBitsData(udBuf, USERDATABUF_SIZE, &ud_bPos, pOct[cnt], bitsOfchari)) {
				dynaFree(pOct);
				goto error;
			}
		}
		dynaFree(pOct);
	}
	else {
		for(cnt=0 ; cnt < bytesOfchari ; cnt++)
		{
			if(0 > setBitsData(udBuf, USERDATABUF_SIZE, &ud_bPos, bData->bd_user_data.log_chari[cnt], bitsOfchari))
				goto error;
		}
	}


	ud_subparam_len = (uint8_t) (ud_bPos/8);
	if(ud_bPos%8)
		ud_subparam_len++;

	param_len = (2 + 3) + (2 + ud_subparam_len); // Message Identifier is fixed to 3 bytes body and 2 bytes header.

	PDUP("CDMA SMS: pduCDMAConvBearerDataToStr ud_bPos=[%d], UserData Length=[%d], BearerData Length=[%d]", ud_bPos, ud_subparam_len, param_len);

	len += pduConvHexOctToStr((const char*)&param_id, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&param_len, 2, (char *) pPdu+len, 4, TRUE);

	//append Message Identifier SubParameter header and body
	subparam_id = 0x00;
	subparam_len = 0x03;
	len += pduConvHexOctToStr((const char*)&subparam_id, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&subparam_len, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&miBuf, 2*subparam_len, (char *) pPdu+len, 4*subparam_len, TRUE);

	//append User Data SubParameter header and body
	subparam_id = 0x01;
	subparam_len = ud_subparam_len;
	len += pduConvHexOctToStr((const char*)&subparam_id, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&subparam_len, 2, (char *) pPdu+len, 4, TRUE);
	len += pduConvHexOctToStr((const char*)&udBuf, 2*subparam_len, (char *) pPdu+len, 4*subparam_len, TRUE);

	return len;

error:
	SYSLOG_ERR("CDMA SMS: pduCDMAConvBearerDataToStr Return Error");
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduGetCDMAAddress(cdma_sms_address_s_type * dest, uint8_t *src, int in_len)
{
	uint32_t bitData = 0;
	int pos = 0;
	int cnt = 0;
	int numBits = 0;

	if (src == NULL || dest == NULL || in_len <= 0)
	{
		return -1;
	}

	if (0 != getBitsData(&bitData, src, in_len, pos, 1))
		goto error;
	pos += 1;
	dest->digit_mode = (cdma_sms_digit_mode_e_type) bitData;

	if (0 != getBitsData(&bitData, src, in_len, pos, 1))
		goto error;
	pos += 1;
	dest->number_mode = (cdma_sms_number_mode_e_type) bitData;

	if (dest->digit_mode == CDMA_SMS_DIGIT_MODE_4_BIT) {
		dest->number_type = CDMA_SMS_NUMBER_MAX32;
		dest->number_plan = CDMA_SMS_NUMBER_PLAN_MAX;
		numBits = 4;
	}
	else if(dest->digit_mode == CDMA_SMS_DIGIT_MODE_8_BIT) {
		numBits = 8;
		if (0 != getBitsData(&bitData, src, in_len, pos, 3))
			goto error;
		pos += 3;
		dest->number_type = (cdma_sms_number_type_e_type) bitData;

		if(dest->number_mode != CDMA_SMS_NUMBER_MODE_NONE_DATA_NETWORK)
			dest->number_plan = CDMA_SMS_NUMBER_PLAN_MAX;
		else {
			if (0 != getBitsData(&bitData, src, in_len, pos, 4))
				goto error;
			pos += 4;
			dest->number_plan = (cdma_sms_number_plan_e_type) bitData;
		}
	}
	else
		goto error;

	if (0 != getBitsData(&bitData, src, in_len, pos, 8))
		goto error;
	pos += 8;
	dest->num_fields = (uint8_t) bitData;
	dest->log_header.cbAddrLen = (unsigned char) bitData;

	if (dest->number_type == CDMA_SMS_NUMBER_INTERNATIONAL)
		dest->log_header.bAddressType = 145;
	else
		dest->log_header.bAddressType = 128;

	memset(dest->log_body, 0x00, MAX_ADDR_LEN+1);
	for (cnt=0; cnt < dest->num_fields; cnt++)
	{
		if (0 != getBitsData(&bitData, src, in_len, pos, numBits))
			goto error;
		pos += numBits;

		if (numBits == 4) {
			if (cnt%2 == 0) {
				if ((bitData & 0x0F) == 0x0A)
					dest->log_body[cnt/2] = (uint8_t) 0x00;
				else
					dest->log_body[cnt/2] = (uint8_t) (bitData & 0x0F);
			}
			else {
				if ((bitData & 0x0F) == 0x0A)
					dest->log_body[cnt/2] |= (uint8_t) 0x00;
				else
					dest->log_body[cnt/2] |= (uint8_t) ((bitData & 0x0F) << 4);
			}
		}
		else {
			dest->log_body[cnt] = (uint8_t)bitData;
		}
#if 0
		// DTMF Conversion rule is from Table 2.7.1.3.2.4-4 Representation of DTMF Digits in "3GPP2 C.S0005-D"
		if (bitData >= 0x01 && bitData <= 0x09)
			dest->chari[cnt] = (uint8_t) '0' + bitData;
		else if (bitData == 0x0A)
			dest->chari[cnt] = (uint8_t) '0';
		else if (bitData == 0x0B)
			dest->chari[cnt] = (uint8_t) '*';
		else if (bitData == 0x0C)
			dest->chari[cnt] = (uint8_t) '#';
		// This is not defined in the standards, but Cinterion module uses this value.
		else if (bitData == 0x00)
			dest->chari[cnt] = (uint8_t) '0';
		else
			goto error;
#endif
	}

	return 0;

error:
	SYSLOG_ERR("CDMA SMS: Decoding Error: Address");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int pduGetCDMASubaddress(cdma_sms_address_s_type * dest, uint8_t *src, int in_len)
{
	if (src == NULL || dest == NULL || in_len <= 0)
	{
		return -1;
	}


	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduDecodeBDMsgId( cdma_sms_bd_msg_id_s_type * dest, uint8_t *src, int in_len)
{
	uint32_t bitData = 0;
	int pos = 0;

	if (src == NULL || dest == NULL || in_len <= 0)
	{
		return -1;
	}

	if (0 != getBitsData(&bitData, src, in_len, pos, 4))
		goto error;
	pos += 4;
	dest->bd_sub_msg_type = (cdma_sms_bd_sub_msg_type_e_type) bitData;

	if (0 != getBitsData(&bitData, src, in_len, pos, 16))
		goto error;
	pos += 16;
	dest->bd_sub_msg_id = (uint16_t) bitData;

	if (0 != getBitsData(&bitData, src, in_len, pos, 1))
		goto error;
	pos += 1;
	dest->bd_sub_header_ind = (uint8_t) bitData;

	return 0;

error:
	SYSLOG_ERR("CDMA SMS: Decoding Error: Bearer Data Message ID");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int pduDecodeBDUserData( cdma_sms_bd_user_data_s_type * dest, uint8_t *src, int in_len)
{
	uint32_t bitData = 0;
	int pos = 0;
	int numOfBits = 0, bytesOfChars = 0, cnt = 0;

	if (src == NULL || dest == NULL || in_len <= 0)
	{
		return -1;
	}

	if (0 != getBitsData(&bitData, src, in_len, pos, 5))
		goto error;
	pos += 5;
	dest->bd_sub_msg_enc = (cdma_sms_bd_sub_msg_enc_e_type) bitData;

	if(dest->bd_sub_msg_enc == CDMA_BD_SUB_MSG_ENC_EXT_PM
		|| dest->bd_sub_msg_enc == CDMA_BD_SUB_MSG_ENC_EXT_GSM)
      	{
		if (0 != getBitsData(&bitData, src, in_len, pos, 8))
			goto error;
		pos += 8;
		dest->bd_sub_msg_type = (uint8_t) bitData;
	}
	else
		dest->bd_sub_msg_type = (uint8_t) 0xFF;

	if (0 != getBitsData(&bitData, src, in_len, pos, 8))
		goto error;
	pos += 8;
	dest->bd_sub_num_fields = (uint8_t) bitData;

	switch (dest->bd_sub_msg_enc)
	{

		case CDMA_BD_SUB_MSG_ENC_7BIT_ACSII:
		case CDMA_BD_SUB_MSG_ENC_IA5:
		case CDMA_BD_SUB_MSG_ENC_7BIT_GSM:
			numOfBits = 7;
			bytesOfChars = dest->bd_sub_num_fields;
			dest->log_dcs.cmd   = 0x04;
			dest->log_dcs.param = 0x04; // DCS_8BIT_CODING
			break;

		case CDMA_BD_SUB_MSG_ENC_8BIT_OCT:
		case CDMA_BD_SUB_MSG_ENC_LATIN_HEB:
		case CDMA_BD_SUB_MSG_ENC_LATIN:
			numOfBits = 8;
			bytesOfChars = dest->bd_sub_num_fields;
			dest->log_dcs.cmd   = 0x04;
			dest->log_dcs.param = 0x04; // DCS_8BIT_CODING
			break;

		case CDMA_BD_SUB_MSG_ENC_UNICODE: //16-bit
			numOfBits = 8;
			bytesOfChars = dest->bd_sub_num_fields * 2;
			dest->log_dcs.cmd   = 0x04;
			dest->log_dcs.param = 0x08; // DCS_UCS2_CODING
			break;

		case CDMA_BD_SUB_MSG_ENC_JIS:
		case CDMA_BD_SUB_MSG_ENC_KOREAN:
			numOfBits = 8;
			bytesOfChars = dest->bd_sub_num_fields;
			dest->log_dcs.cmd   = 0x04;
			dest->log_dcs.param = 0x04; // DCS_8BIT_CODING
			break;

		case CDMA_BD_SUB_MSG_ENC_EXT_GSM:
		case CDMA_BD_SUB_MSG_ENC_EXT_PM:
		default:
			SYSLOG_ERR("cdma sms: not supportted encoding type [%d]",dest->bd_sub_msg_enc);
			goto error;
			break;
	}

	PDUP("CDMA SMS: Parse UserData bd_sub_num_fields=[%d], numOfBits=[%d], bytesOfChars=[%d]", dest->bd_sub_num_fields, numOfBits, bytesOfChars);

	dest->log_cbUserData = bytesOfChars;
	memset(dest->log_chari, 0x00, MAX_USER_DATA_LEN);

	for(cnt=0; cnt < bytesOfChars; cnt++)
	{
		if (0 != getBitsData(&bitData, src, in_len, pos, numOfBits))
			goto error;
		pos += numOfBits;
		dest->log_chari[cnt] = (uint8_t) bitData;
	}
	return 0;

error:
	SYSLOG_ERR("CDMA SMS: Decoding Error: UserData");
	return -1;
}
///////////////////////////////////////////////////////////////////////////////
int pduDecodeTimeStamp( cdma_sms_bd_timestamp_s_type * dest, uint8_t *src, int in_len)
{
	if (src == NULL || dest == NULL || in_len != 6) // length of timestamp param should be 6.
	{
		return -1;
	}

	memcpy(dest, src, 6);

	dest->log_timestamp.bYear = ((dest->year >> 4) & 0x0F) | ((dest->year << 4) & 0xF0);
	dest->log_timestamp.bMonth = ((dest->month >> 4) & 0x0F) | ((dest->month << 4) & 0xF0);
	dest->log_timestamp.bDay = ((dest->day >> 4) & 0x0F) | ((dest->day << 4) & 0xF0);
	dest->log_timestamp.bHour = ((dest->hours >> 4) & 0x0F) | ((dest->hours << 4) & 0xF0);
	dest->log_timestamp.bMinute = ((dest->minutes >> 4) & 0x0F) | ((dest->minutes << 4) & 0xF0);
	dest->log_timestamp.bSecond = ((dest->seconds >> 4) & 0x0F) | ((dest->seconds << 4) & 0xF0);
	dest->log_timestamp.bTZ = 0;

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
int pduDecodeBearerData(cdma_sms_bearer_data_s_type * dest, uint8_t *src, int in_len)
{
	uint8_t subparameter_id = 0xFF, subparameter_len = 0xFF;
	uint8_t * s_pos, * e_pos;
	uint8_t buffer[512];

	if (src == NULL || dest == NULL || in_len <= 0)
	{
		return -1;
	}

	s_pos = src;
	e_pos = src + in_len;

	for ( ; s_pos < e_pos ; )
	{
		s_pos += pduGetCDMAPduData(s_pos, &subparameter_id, sizeof(subparameter_id));
		s_pos += pduGetCDMAPduData(s_pos, &subparameter_len, sizeof(subparameter_len));

		s_pos += pduGetCDMAPduData(s_pos, buffer, subparameter_len);

		switch (subparameter_id)
		{
			case 0x00:  // Message Identifier
				PDUP("CDMA SMS-BD: Message Identifier LEN=[%d]", subparameter_len);
				if (0 != pduDecodeBDMsgId(&dest->bd_msg_id, buffer, subparameter_len))
					return -1;
				PDUP("CDMA SMS-BD: db_sub_msg_type =[%d]", dest->bd_msg_id.bd_sub_msg_type);
				PDUP("CDMA SMS-BD: bd_sub_msg_id=[%d]", dest->bd_msg_id.bd_sub_msg_id);
				PDUP("CDMA SMS-BD: bd_sub_header_ind=[%d]", dest->bd_msg_id.bd_sub_header_ind);
				if (dest->bd_msg_id.bd_sub_msg_type != CDMA_BD_SUB_MSG_TYPE_DELIVER
					&& dest->bd_msg_id.bd_sub_msg_type != CDMA_BD_SUB_MSG_TYPE_SUBMIT)
					return -1;
				break;

			case 0x01:  // User Data
				PDUP("CDMA SMS-BD: User Data LEN=[%d]", subparameter_len);
				if (0 != pduDecodeBDUserData(&dest->bd_user_data, buffer, subparameter_len))
					return -1;
				PDUP("CDMA SMS-BD: bd_sub_msg_enc=[%d]", dest->bd_user_data.bd_sub_msg_enc);
				PDUP("CDMA SMS-BD: bd_sub_msg_type=[%d]", dest->bd_user_data.bd_sub_msg_type);
				PDUP("CDMA SMS-BD: bd_sub_num_fields=[%d]", dest->bd_user_data.bd_sub_num_fields);
				PDUP("CDMA SMS-BD: log_chari=[%s]", dest->bd_user_data.log_chari);
				break;

			case 0x03:  // Message Center Time Stamp
				PDUP("CDMA SMS-BD: Message Center Time Stamp LEN=[%d]", subparameter_len);
				if (0 != pduDecodeTimeStamp(&dest->bd_timestamp, buffer, subparameter_len))
					return -1;
				PDUP("CDMA SMS-BD: year=[%02x]", dest->bd_timestamp.year);
				PDUP("CDMA SMS-BD: month=[%02x]", dest->bd_timestamp.month);
				PDUP("CDMA SMS-BD: day=[%02x]", dest->bd_timestamp.day);
				PDUP("CDMA SMS-BD: hours=[%02x]", dest->bd_timestamp.hours);
				PDUP("CDMA SMS-BD: minutes=[%02x]", dest->bd_timestamp.minutes);
				PDUP("CDMA SMS-BD: seconds=[%02x]", dest->bd_timestamp.seconds);
				break;

			default:
				SYSLOG_DEBUG("CDMA SMS-BD: Ignored type of db sub param: 0x%02x", subparameter_id);
				break;
		}
	}

	return 0;
}
///////////////////////////////////////////////////////////////////////////////
struct log_sms_message* pduCreateCDMALogSMSMessage(const char* szStrSMSMessage, tpmti_field_type msg_type)
{
	// get
	struct log_sms_message* pLSM = dynaCreate(sizeof(struct log_sms_message), pduFreeLogSMSMessage);
	unsigned char *pOct, *pStart, *pEnd;
	int lenOfpOct;

	char *tmp_str;
	struct tm* ts = 0;
	sms_encoding_type coding_type;

	cdma_sms_message_s_type * cdma_message;
	uint8_t cdma_msg_type = 0xff;
	uint8_t parameter_id = 0xff;
	uint8_t parameter_len = 0xff;
	uint8_t buffer[512];

	if (!pLSM)
		goto error;

	PDUP("CDMA SMS: pduCreateCDMALogSMSMessage szStrSMSMessage=[%s]", szStrSMSMessage);

	pLSM->pRawSMSMessage = dynaCreate(sizeof(cdma_sms_message_s_type), 0);
	if (!pLSM->pRawSMSMessage)
		goto error;

	cdma_message = pLSM->pRawSMSMessage;

	memset (buffer, 0xff, 512);

	// get raw SMS message
	pStart = pOct = (unsigned char *) pduCreateOctByStr(szStrSMSMessage, (int *)&lenOfpOct);
	if (!pOct)
		goto error1;

	pEnd = pOct + lenOfpOct;

	pOct += pduGetCDMAPduData((uint8_t *)pOct, &cdma_msg_type, sizeof(cdma_msg_type));

	if (cdma_msg_type != 0x00) {
		SYSLOG_ERR("Only support CDMA PtoP SMS Message - Message Type:%d", cdma_msg_type);
		goto error;
	}

	for (; pOct <pEnd -1 ; )
	{
		pOct += pduGetCDMAPduData((uint8_t *)pOct, &parameter_id, sizeof(parameter_id));
		pOct += pduGetCDMAPduData((uint8_t *)pOct, &parameter_len, sizeof(parameter_len));

		pOct += pduGetCDMAPduData((uint8_t *)pOct, buffer, parameter_len);

		switch(parameter_id)
		{
			case PARAM_ID_TELESERVICE_ID:
				if (parameter_len != 2)
				{
					SYSLOG_ERR("ERROR: Parameter lengh of Teleservice id is incorrect len=%d", parameter_len);
					goto error;
				}
				cdma_message->teleservice_id = (cdma_sms_teleservice_id_e_type) (buffer[0] << 0x08 |  buffer[1]);
				SYSLOG_DEBUG("CDMA SMS: Teleservice Identifier=[%d]", cdma_message->teleservice_id);
				break;

			case PARAM_ID_SERVICE_CATEGORY:
				if (parameter_len != 2)
				{
					SYSLOG_ERR("ERROR: Parameter lengh of Service Category is incorrect len=%d", parameter_len);
					goto error;
				}
				cdma_message->serv_category = (uint16_t) (buffer[0] << 0x08 |  buffer[1]);
				break;

			case PARAM_ID_ORIG_ADDR:
			case PARAM_ID_DEST_ADDR:
				pduGetCDMAAddress((cdma_sms_address_s_type *) &cdma_message->address, buffer, parameter_len);
				PDUP("CDMA SMS: ADDRESS PARAMETER TYPE=[%d]", parameter_id);
				PDUP("CDMA SMS: digit_mode=[0x%02x], number_mode=[0x%02x]", cdma_message->address.digit_mode, cdma_message->address.number_mode);
				PDUP("CDMA SMS: number_type=[0x%02x], number_plan=[0x%02x]", cdma_message->address.number_type, cdma_message->address.number_plan);
				PDUP("CDMA SMS: Logical ADDR cbAddrLen=[%d],bAddressType =[%d]", cdma_message->address.log_header.cbAddrLen, cdma_message->address.log_header.bAddressType);
				break;

			case PARAM_ID_BEARER_REPLY:
				if (parameter_len != 1)
				{
					SYSLOG_ERR("ERROR: Parameter lengh of Bearer Reply Option is incorrect len=%d", parameter_len);
					goto error;
				}
				cdma_message->reply_seq = (uint8_t) (buffer[0] >> 0x02);
				PDUP("CDMA SMS:  PARAM_ID_BEARER_REPLY=[%d]",cdma_message->reply_seq );
				break;

			case PARAM_ID_BEARER_DATA:
				pduDecodeBearerData(&cdma_message->bearer_data, buffer, parameter_len);
				break;

			case PARAM_ID_ORIG_SUBADDR:
			case PARAM_ID_DEST_SUBADDR:
			case PARAM_ID_CAUSE_CODES:
				SYSLOG_DEBUG("CDMA SMS: Ignored Parameter ID: 0x%02x", parameter_id);
				break;

			default:
				SYSLOG_ERR("ERROR: INVALID PARAMETER ID: 0x%02x", parameter_id);
				break;
		}
	}

	// set SMSC address with NULL data
	static const struct addr_fields dummy_SMSC = { 0x00, SMSC_NUMBER };
	pLSM->pSMSCNumber = &dummy_SMSC;
	PDUP("PDU SMSC: len %d", pLSM->pSMSCNumber->cbAddrLen);
	PDUP("PDU SMSC: type %d", pLSM->pSMSCNumber->bAddressType);
	tmp_str = pduCreateHumanAddr(pLSM->pSMSCNumber, SMSC_NUMBER);
	PDUP("PDU SMSC: addr %s", tmp_str);

	if (cdma_message->bearer_data.bd_msg_id.bd_sub_msg_type == CDMA_BD_SUB_MSG_TYPE_DELIVER)
	{
		pLSM->msg_type.smsDeliverType.mti = SMS_DELIVER;
		pLSM->msg_type.smsDeliverType.mms = NO_MORE_MSG;
		pLSM->msg_type.smsDeliverType.lp = 0;
		pLSM->msg_type.smsDeliverType.sri = 0;
		pLSM->msg_type.smsDeliverType.udhi = 0;
		pLSM->msg_type.smsDeliverType.rp = 0;
	}
	else if (cdma_message->bearer_data.bd_msg_id.bd_sub_msg_type == CDMA_BD_SUB_MSG_TYPE_SUBMIT)
	{
		pLSM->msg_type.smsDeliverType.mti = SMS_SUBMIT;
		pLSM->msg_type.smsSubmitType.rp = 0;
		pLSM->msg_type.smsSubmitType.vpf = NO_VPF;
		pLSM->msg_type.smsSubmitType.srr = 0;
		pLSM->msg_type.smsSubmitType.udhi = 0;
		pLSM->msg_type.smsSubmitType.rp = 0;
	}

	if (pLSM->msg_type.smsDeliverType.mti != msg_type) {
		SYSLOG_ERR("mti mismatch: parse %d, expect %d", pLSM->msg_type.smsDeliverType.mti, msg_type);
		pLSM->msg_type.smsDeliverType.mti = msg_type;
	}

	if (pLSM->msg_type.smsDeliverType.mti == SMS_DELIVER ||
		pLSM->msg_type.smsDeliverType.mti == SMS_RESERVED_TYPE) {
		PDUP("PDU HEADER: SMS_DELIVER");
		PDUP("   TP-MTI : %d", pLSM->msg_type.smsDeliverType.mti);
		PDUP("   TP-MMS : %d", pLSM->msg_type.smsDeliverType.mms);
		PDUP("   TP-LP  : %d", pLSM->msg_type.smsDeliverType.lp);
		PDUP("   TP-SRI : %d", pLSM->msg_type.smsDeliverType.sri);
		PDUP("   TP-UDHI: %d", pLSM->msg_type.smsDeliverType.udhi);
		PDUP("   TP-RP  : %d", pLSM->msg_type.smsDeliverType.rp);
		// get originating address
		pLSM->addr.pOrigAddr = (struct addr_fields*) &cdma_message->address.log_header;
		tmp_str = pduCreateHumanAddr(pLSM->addr.pOrigAddr, MOBILE_NUMBER);
		PDUP("CDMA SMS: Address: [%s]", tmp_str);
	} else if (pLSM->msg_type.smsDeliverType.mti == SMS_SUBMIT) {
		PDUP("PDU HEADER: SMS_SUBMIT");
		PDUP("   TP-MTI : %d", pLSM->msg_type.smsSubmitType.mti);
		PDUP("   TP-RD  : %d", pLSM->msg_type.smsSubmitType.rp);
		PDUP("   TP-VPF : %d", pLSM->msg_type.smsSubmitType.vpf);
		PDUP("   TP-SRR : %d", pLSM->msg_type.smsSubmitType.srr);
		PDUP("   TP-UDHI: %d", pLSM->msg_type.smsSubmitType.udhi);
		PDUP("   TP-RP  : %d", pLSM->msg_type.smsSubmitType.rp);
		// get message reference
		pLSM->SubmitMsgRef = cdma_message->bearer_data.bd_msg_id.bd_sub_msg_id;
		if (pLSM->SubmitMsgRef > last_used_tpmr)
			last_used_tpmr = pLSM->SubmitMsgRef;

		// get destination address
		pLSM->addr.pDestAddr = (struct addr_fields*) &cdma_message->address.log_header;
	} else {
		PDUP("Not suppoted message type %d", pLSM->msg_type.smsDeliverType.mti);
		goto error;
	}

	pLSM->dwPID = 0; 
	PDUP("PDU TP-PID: 0x%02x", pLSM->dwPID);

	// dcs
	memset(&pLSM->dcs, 0x00, sizeof(struct sms_dcs));
	pLSM->dcs = cdma_message->bearer_data.bd_user_data.log_dcs;
	PDUP("PDU TP-DCS: param-0x%02x, cmd-0x%02x", pLSM->dcs.param,pLSM->dcs.cmd );

	if (pLSM->msg_type.smsDeliverType.mti == SMS_SUBMIT) {
#if 0 // TODO: CDMA has this function, need to be done.
		// get validity period
		if (pLSM->msg_type.smsSubmitType.vpf == RELATIVE_VPF_FORMAT) {
			pLSM->SubmitValidPeriod.rel_tpvp = *pOct;
			PDUP("PDU REL-TPVP: 0x%04x", *pOct);
			pOct++;
		} else if (pLSM->msg_type.smsSubmitType.vpf == ABSOLUTE_VPF_FORMAT) {
			pLSM->SubmitValidPeriod.abs_tpvp = (struct timestamp_fields*)pOct;
			PDUP("PDU ABS-TPVP: %02x%02x%02x%02x%02x%02x%02x", *pOct, *(pOct+1), *(pOct+2), *(pOct+3), *(pOct+4), *(pOct+5), *(pOct+6));
			pOct += sizeof(struct timestamp_fields);
		} else if (pLSM->msg_type.smsSubmitType.vpf == ENHANCED_VPF_FORMAT) {
			pLSM->SubmitValidPeriod.enh_tpvp = (struct enhanced_tpvp_fields*)pOct;
			PDUP("PDU ENH-TPVP: %02x%02x%02x%02x%02x%02x%02x", *pOct, *(pOct+1), *(pOct+2), *(pOct+3), *(pOct+4), *(pOct+5), *(pOct+6));
			pOct += sizeof(struct timestamp_fields);
		}
#endif
	} else {
		// get timestamp
		pLSM->pTimeStamp = (struct timestamp_fields*)&cdma_message->bearer_data.bd_timestamp.log_timestamp;
		ts = pduCreateLinuxTime(pLSM->pTimeStamp);
		PDUP("PDU TP-SCTS: %04d/%02d/%02d,%02d:%02d:%02d",
					ts->tm_year, ts->tm_mon, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
	}

	// get user data
	pLSM->cbUserData = cdma_message->bearer_data.bd_user_data.log_cbUserData;
	pLSM->pUserData = &cdma_message->bearer_data.bd_user_data.log_chari;

	coding_type = parse_msg_coding_type((struct sms_dcs *)&pLSM->dcs);
	PDUP("CDMA SMS: coding_type = %d", coding_type);
	PDUP("CDMA SMS: pLSM->cbUserData = %d", pLSM->cbUserData);
	dynaFree(pStart);
	return pLSM;

error1:
	dynaFree(pStart);
error:
	dynaFree(pLSM);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
int createCDMAPduData(char *destno, int int_no, int ucs_coding, unsigned char *pEncodeMsg, int cbEncodeMsg,
		unsigned char *pPduData, struct addr_fields *smsc_addr, int udh_len)
{
	struct log_sms_message* pLSM = dynaCreate(sizeof(struct log_sms_message), pduFreeLogSMSMessage);
	unsigned char* pPdu = pPduData;
	cdma_sms_message_s_type * pCdmaMsg = dynaCreate(sizeof(cdma_sms_message_s_type), 0);
	int encoded_msg_len = 0;
	static unsigned int message_id = 0;

	if (!pLSM || !pCdmaMsg)
		goto error;

	PDUP("CDMA SMS: destno=[%s], int_no=[%d]", destno, int_no);
	PDUP("CDMA SMS: ucs_coding=[%d], pEncodeMsg=[%s]", ucs_coding, pEncodeMsg);
	PDUP("CDMA SMS: cbEncodeMsg=[%d], udh_len=[%d]",cbEncodeMsg, udh_len);
	PDUP("CDMA SMS: smsc_addr: cbAddrLen=[%d], bAddressType=[%d]",smsc_addr->cbAddrLen, smsc_addr->bAddressType);

//------------------------------------------------------------ ------------------------------------------------------------
	memset (pCdmaMsg, 0, sizeof(cdma_sms_message_s_type));
	pCdmaMsg->is_mo = 1;
	pCdmaMsg->teleservice_id = CDMA_SMS_TELE_ID_CMT_95;

	if(int_no)
	{
		pCdmaMsg->address.digit_mode = CDMA_SMS_DIGIT_MODE_8_BIT;
		pCdmaMsg->address.number_mode = CDMA_SMS_NUMBER_MODE_NONE_DATA_NETWORK; // CDMA_SMS_NUMBER_MODE_NONE_DATA_NETWORK;
		pCdmaMsg->address.number_type = CDMA_SMS_NUMBER_INTERNATIONAL;
		pCdmaMsg->address.number_plan = CDMA_SMS_NUMBER_PLAN_ISDN; //not sure
		pCdmaMsg->address.num_fields = (uint8_t) strlen(destno);
		strcpy((char *)pCdmaMsg->address.chari, destno);
	}
	else
	{
		pCdmaMsg->address.digit_mode = CDMA_SMS_DIGIT_MODE_4_BIT;
		pCdmaMsg->address.number_mode = CDMA_SMS_NUMBER_MODE_DATA_NETWORK; // CDMA_SMS_NUMBER_MODE_NONE_DATA_NETWORK;
		pCdmaMsg->address.number_type = CDMA_SMS_NUMBER_UNKNOWN;
		pCdmaMsg->address.number_plan = CDMA_SMS_NUMBER_PLAN_UNKNOWN;
		pCdmaMsg->address.num_fields = (uint8_t) strlen(destno);
		strcpy((char *)pCdmaMsg->address.chari, destno);
	}

	pCdmaMsg->bearer_data.bd_msg_id.bd_sub_msg_type = CDMA_BD_SUB_MSG_TYPE_SUBMIT;

	// requirement of C.S0015, 4.3.1.5 Setting of Message Identifier Field
	message_id += 1;
	if (message_id > 65535)
		message_id %= 65536;
	pCdmaMsg->bearer_data.bd_msg_id.bd_sub_msg_id = message_id;
	pCdmaMsg->bearer_data.bd_msg_id.bd_sub_header_ind = 0;

	pCdmaMsg->bearer_data.bd_user_data.bd_sub_msg_enc = (ucs_coding?CDMA_BD_SUB_MSG_ENC_UNICODE:CDMA_BD_SUB_MSG_ENC_7BIT_ACSII) ;
	pCdmaMsg->bearer_data.bd_user_data.bd_sub_msg_type = 0;
	if (ucs_coding) {
		encoded_msg_len = (strlen((char *)pEncodeMsg)/4);
	}
	else {
		if (udh_len)
			encoded_msg_len = (cbEncodeMsg-12)+udh_len/2+1;
		else
			encoded_msg_len = cbEncodeMsg;
	}
	pCdmaMsg->bearer_data.bd_user_data.bd_sub_num_fields = (uint8_t) encoded_msg_len;
	strcpy((char *)pCdmaMsg->bearer_data.bd_user_data.log_chari, (char *)pEncodeMsg+udh_len);

	PDUP("CDMA SMS: Address digit_mode=[%d], number_mode=[%d]",pCdmaMsg->address.digit_mode,pCdmaMsg->address.number_mode);
	PDUP("CDMA SMS: Address number_type=[%d], number_plan=[%d]",pCdmaMsg->address.number_type,pCdmaMsg->address.number_plan);
	PDUP("CDMA SMS: Address num_fields=[%d], chari=[%s]",pCdmaMsg->address.num_fields,pCdmaMsg->address.chari);
	PDUP("CDMA SMS: User Data num_fields=[%d], chari=[%s]", pCdmaMsg->bearer_data.bd_user_data.bd_sub_num_fields, pCdmaMsg->bearer_data.bd_user_data.log_chari);

	/* -------------------------------------------------------------------- *
	 * Parameter ID								*
	 * 	-(*) Teleservice Identifier	--> '00000000' (0x00)		*
	 * 	- Service Category		--> '00000001' (0x01)		*
	 * 	- Originating Address		--> '00000010' (0x02)		*
	 * 	- Originating Subaddress	--> '00000011' (0x03)		*
	 * 	-(*) Destination Address	--> '00000100' (0x04)		*
	 * 	- Destination Subaddress	--> '00000101' (0x05)		*
	 * 	- Bearer Reply Option		--> '00000110' (0x06)		*
	 * 	- Cause Codes			--> '00000111' (0x07)		*
	 * 	-(*) Bearer Data		--> '00001000' (0x08)		*
	 * -------------------------------------------------------------------- *
	 * Bearer Data SubParameter ID						*
	 * 	-(*) Message Identifier			--> '00000000' (0x00)	*
	 * 	-(*) User Data				--> '00000001' (0x01)	*
	 * 	- User Response Code			--> '00000010' (0x02)	*
	 * 	- Message Center Time Stamp		--> '00000011' (0x03)	*
	 * 	- Validity Period - Absolute		--> '00000100' (0x04)	*
	 * 	- Validity Period - Relative		--> '00000101' (0x05)	*
	 * 	- Deferred Delivery Time - Absolute	--> '00000110' (0x06)	*
	 * 	- Deferred Delivery Time - Relative	--> '00000111' (0x07)	*
	 * 	- Priority Indicator			--> '00001000' (0x08)	*
	 * 	- Privacy Indicator			--> '00001001' (0x09)	*
	 * 	- Reply Option				--> '00001010' (0x0A)	*
	 * 	- Number of Messages			--> '00001011' (0x0B)	*
	 * 	- Alert on Message Delivery		--> '00001100' (0x0C)	*
	 * 	- Language Indicator			--> '00001101' (0x0D)	*
	 * 	- Call-Back Number			--> '00001110' (0x0E)	*
	 * 	- Message Display Mode			--> '00001111' (0x0F)	*
	 * 	- Multiple Encoding User Data		--> '00010000' (0x10)	*
	 * 	- Message Deposit Index			--> '00010001' (0x11)	*
	 * 	- Service Category Program Data		--> '00010010' (0x12)	*
	 * 	- Service Category Program Results	--> '00010011' (0x13)	*
	 * 	- Message Status			--> '00010100' (0x14)	*
	 * 	- TP-Failure Cause			--> '00010101' (0x15)	*
	 * 	- Enhanced VMN				--> '00010110' (0x16)	*
	 * 	- Enhanced VMN Ack			--> '00010111' (0x17)	*
	 * -------------------------------------------------------------------- */

	// set SMS_MSG_TYPE
	strncpy((char *)pPdu, "00", 2); //0x00 (Point to Point)
	pPdu += 2;

	// Set Teleservice Identifier
	strncpy((char *)pPdu, "0002", 4); // Param_id: Teleservice Identifier(0x00) and Param_LEN: 0x02
	pPdu += 4;
	pPdu += pduCDMAConvVoidOctToStr(pPdu,&pCdmaMsg->teleservice_id, 2);

	// Set Destination Address
	pPdu += pduCDMAConvDestAddrToStr(pPdu, &pCdmaMsg->address);

	// Set Bearer Data
	pPdu += pduCDMAConvBearerDataToStr(pPdu, &pCdmaMsg->bearer_data);
	
	SYSLOG_ERR("CDAM SMS: pPduData=[%s]", pPduData);

	dynaFree(pCdmaMsg);
	dynaFree(pLSM);
	return (strlen((char *)pPduData)/2);

error:
	dynaFree(pCdmaMsg);
	dynaFree(pLSM);
	return 0;
}
