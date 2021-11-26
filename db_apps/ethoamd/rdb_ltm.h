#pragma once


///|Dot1ag.{1-4}.Ltm.send||Bool||W|| ||false||
///:true: '''send a LTM'''
///:false: LTM sent.
#define LTM_send	"dot1ag.lmp.ltm.send"

///|Dot1ag.{1-4}.Ltm.rmpid||Unsigned int||W||P||0||Identification of the destination MP instance
///:(0..8191)
///:0 --- used Dot1ag.{1-4}.Ltm.destmac only
#define LTM_rmpid	"dot1ag.lmp.ltm.rmpid"

///|Dot1ag.{1-4}.Ltm.destmac||String||W||P|| ||Mac address of the LTM destination in ‘XX:XX:XX:XX:XX:XX’ format.
#define LTM_destmac	"dot1ag.lmp.ltm.destmac"

///|Dot1ag.{1-4}.Ltm.flag||Unsigned int||W||P||0||LTM flag field value to send as part of the message (0..255)
#define LTM_flag	"dot1ag.lmp.ltm.flag"

///|Dot1ag.{1-4}.Ltm.ttl||Unsigned int||W||P||64||LTM TTL value to use
///(0..255) Default value is 64
///Note: If ‘0’ is used in this field, NO LTR will be returned.
#define LTM_ttl	"dot1ag.lmp.ltm.ttl"

///|Dot1ag.{1-4}.Ltm.timeout||Unsigned int||W||P||5000||LTR timeout in msec
#define LTM_timeout	"dot1ag.lmp.ltm.timeout"

///|Dot1ag.{1-4}.Ltm.LTMtransID||Unsigned int||W|| ||1||Current LTM Transaction Identifier. it starts from 1.
///(1..MAXUINT)
#define LTM_LTMtransID	"dot1ag.lmp.ltm.ltmtransid"

///|Dot1ag.{1-4}.Ltm.Status||String||-|| || ||Current LTM operation status
///*Success: LTM is sent
///*Success: LTM is completed
///*Error: Cannot set LTM parameter xxx
///*Error: Cannot get LTM parameter xxx
///*Error: 802.1ag is not started
///-- possible reason could be found from 'Dot1ag.{1-4}.Mda.status'
///#MDA configuration does not exist
///#Cannot find device: the interface does not exist at all
///#Cannot bind to device:the interface is valid
///#MDA configuration is incorrect:MD/MA format, MDLevel parameter
///*Error: cannot setup LMP end point xxxx
///-- possible reason
///#LMPID is invalid
///#802.1ag is not started
///*Error: RMP xxxx object has not setup yet
///*Error: cannot setup RMP end point xxxx
///*Error: Invalid MAC address
///*Error: cannot start LTM test
///*Error: cannot release LTM test
#define LTM_Status	"dot1ag.lmp.ltm.status"



#include "session.h"



//when LTM_sendLTM is set, read rdb parameters fill into config file, then start to send
//$  0 -- success
//$ <0 --- error code
int sendltm_start(Session *pSession);

// once the sending finish,
int sendltm_end(Session *pSession);
