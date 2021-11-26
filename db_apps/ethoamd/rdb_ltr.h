#pragma once

///|Dot1ag.{1-4}.Ltr.ltmTransId||Unsigned int||-|| ||0||Associated LTM transaction ID
#define LTR_ltmTransId	"dot1ag.lmp.ltr.ltmtransid"

///|Dot1ag.{1-4}.Ltr.rmpid||String||-|| || ||Identification of the remote MP instance sent this reply
///value range is 1-8191
///''multi-LTR result is separated by ','''
#define LTR_rmpid	"dot1ag.lmp.ltr.rmpid"

///|Dot1ag.{1-4}.Ltr.srcmac||String||-|| || ||Source mac address of the remote MP sent the LTR frame in ‘XX:XX:XX:XX:XX:XX’ format.
///''multi-LTR result is separated by ','''
#define LTR_srcmac	"dot1ag.lmp.ltr.srcmac"

///|Dot1ag.{1-4}.Ltr.flag||String||-|| || ||LTR flag field value received
///''multi-LTR result is separated by ','''
#define LTR_flag	"dot1ag.lmp.ltr.flag"

///|Dot1ag.{1-4}.Ltr.relayaction||String||-|| || ||LTR RelayAction value received
///value range is 1-3 as specified in the Table 21-27 in IEEE 802.1ag.
///# RlyHit (reached the destination)
///# Egress port found using the filter database (relayed)
///# Egress port found using the CCM database (relayed)
///''multi-LTR result is separated by ','''
#define LTR_relayaction	"dot1ag.lmp.ltr.relayaction"

///|Dot1ag.{1-4}.Ltr.ttl||String||-|| || ||LTR TTL value received. It should be the LTM TTL value received by the remote MP minus one.
///value range is 0-254
///''multi-LTR result is separated by ','''
#define LTR_ttl	"dot1ag.lmp.ltr.ttl"




#define LTR_lasterr		"dot1ag.lmp.ltr.lasterr"

#include "session.h"

//
int init_ltr(Session *pSession);

int end_ltr(Session *pSession);

// collect ltr parameters
int collect_ltr(Session *pSession);
// update ltr rdb
int update_rdb_ltr(Session *pSession);

