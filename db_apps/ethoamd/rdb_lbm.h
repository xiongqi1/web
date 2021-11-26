///|Dot1ag.{1-4}.Lbm.LBMsToSend||Unsigned int||W|| ||0||The number of LBMs to send
///:(0..64)
///:0: No LBM sends
///:Others: Number of LBMs to send '''Invoke LBM sending'''
#define LBM_LBMsToSend	"dot1ag.lmp.lbm.lbmstosend"

///|Dot1ag.{1-4}.Lbm.rmpid||Unsigned int||W||P||0||Identification of the destination MP instance
///:(0..8191)
///:0 --- used Dot1ag.{1-4}.Ltm.destmac only
#define LBM_rmpid	"dot1ag.lmp.lbm.rmpid"

///|Dot1ag.{1-4}.Lbm.destmac||String||W||P|| ||Mac address of the LBM destination in ‘XX:XX:XX:XX:XX:XX’ format.
#define LBM_destmac	"dot1ag.lmp.lbm.destmac"

///|Dot1ag.{1-4}.Lbm.timeout||Unsigned int||W||P||5000||LBR timeout in msec
///Default value is 5000msec
#define LBM_timeout	"dot1ag.lmp.lbm.timeout"

///|Dot1ag.{1-4}.Lbm.rate||Unsigned int||W||P||1000||LBM sending rate in msec
///Default value is 1000msec
#define LBM_rate	"dot1ag.lmp.lbm.rate"

///|Dot1ag.{1-4}.Lbm.LBMtransID||Unsigned int||W|| ||1||Current LBM Transaction Identifier,
///(1..MAXUINT). it starts from 1. 
#define LBM_LBMtransID	"dot1ag.lmp.lbm.lbmtransid"

///|Dot1ag.{1-4}.Lbm.TLVDataLen||Unsigned int||W||P||0|| Data TLV length (0..1480). 
///The maximum length "1480" is valid only when "unid.max_frame_size" is larger than 1522. 
#define LBM_TLVDataLen	"dot1ag.lmp.lbm.tlvdatalen"

///|Dot1ag.{1-4}.Lbm.Status||String||-|| || ||Current LBM operation status
///*Success: LBM is sent
///*Success: LBM is completed
///*Failed: LBM has no reply
///*Error:  802.1ag is not started
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
///*Error: Cannot setup RMP end point xxxx
///*Error: Cannot start LBM test
///*Error: Cannot set LBM parameter xxx
///*Error: Cannot get LBM parameter xxx
#define LBM_Status	"dot1ag.lmp.lbm.status"





#include "session.h"


//when LBM_sendLBM is set, read rdb parameters fill into config file, then start to send
//$  0 -- success
//$ <0 --- error code
int sendlbm_start(Session *pSession);

// once the sending finish,
int sendlbm_end(Session *pSession);

// check received packet count,
int count_recv_packet(Session *pSession);

