#pragma once

#define TLV_SENDERID	"8021ag.tlv.senderid"

#define TLV_ORGSPEC		"8021ag.tlv.orgspec"

#include "session.h"


int parse_senderid_tlv(SENDERID_TLV *senderid_tlv, const char* str);
int parse_orgspec_tlv(ORGSPEC_TLV *orgspec_tlv,  const char* str);

