

#include "rdb_comms.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "tms.h"
#include "utils.h"
#include <string.h>

// convert hex str to hex
//$ how many byte converted,
int str2hex(unsigned char *dest, int max_byte, const char*str, const char **pEnd)
{
	int n;
	char tmp[3]={0,0,0};
	const unsigned char*p = (const unsigned char*)str;
	// ignore leading space
	while(*p && isspace(*p)) p++;
	for(n=0; n< max_byte; n++)
	{
		if( !isxdigit(*p)) goto lab_end;
		tmp[0] =*p++;
		if( !isxdigit(*p))
		{
			n = -1;
			goto lab_end;
		}
		tmp[1] =*p++;
		dest[n] = strtoul(tmp, NULL, 16);
	}

lab_end:

	if(pEnd) pEnd = p;
	return n;
}

#define LOAD_HEX(v, l)\
	num = str2hex(v, l, p, &pEnd);\
	if (num <0) return -1;\
	p= pEnd;

#define SKIP_COMMA() \
		p = strchr(p, ',');\
		if(p ==0) return -1;

//format(hex): classtype, classid, mgmtDom, mgmtAdr
//example: 10, 01020304, 1234, abcd
//$0 -- success
//-1 -- failed or no conext
int parse_senderid_tlv(SENDERID_TLV *senderid_tlv, const char* str)
{
	int num;
	const char * pEnd;
	const char *p=str;
	memset(senderid_tlv, 0, sizeof(SENDERID_TLV));
	// class type
	LOAD_HEX(&senderid_tlv.chassIDsubtype, 1);
	p = strchr(p, ',');
	if(p ==0) return -1;
	// class id
	LOAD_HEX(senderid_tlv.chassID, 255);
	senderid_tlv.chassIDlength = num;
	p = strchr(p, ',');
	if(p ==0) return 0;

	// mgmtDom
	LOAD_HEX(senderid_tlv.mgmtDom, 255);
	senderid_tlv.mgmtDomLength= num;
	p = strchr(p, ',');
	if(p ==0) return 0;

	// mgmtAdr
	LOAD_HEX(senderid_tlv.mgmtAdr, 255);
	senderid_tlv.mgmtAdrLength= num;
	return 0;
}

//format(hex): oui, type, value
//example: 0102ff, 01, 1020304050a0b0
//$0 -- success
//-1 -- failed or no conext
int parse_orgspec_tlv(ORGSPEC_TLV *orgspec_tlv,  const char* str)
{
	int num;
	const char * pEnd;
	const char *p=str;
	memset(orgspec_tlv, 0, sizeof(ORGSPEC_TLV));
	// oui
	LOAD_HEX(orgspec_tlv.oui, 3);
	orgspec_tlv.totalLen = num
	p = strchr(p, ',');
	if(p ==0) return -1;

	LOAD_HEX(&orgspec_tlv.subType, 1);
	orgspec_tlv.totalLen ++;
	p = strchr(p, ',');
	if(p ==0) return 0;

	LOAD_HEX(orgspec_tlv.value, 1480-4);
	orgspec_tlv.totalLen += num;
	return 0;

}

