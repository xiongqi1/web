#pragma once
#ifdef NCI_Y1731

///|Y1731.{i}.Lmm.Send||Unsigned int||W||0||'''Number of LMM message to send'''. value range:1-1024.
#define Y1731_Lmm_Send	"y1731.lmp.lmm.send"

///|Y1731.{i}.Lmm.Rate||Unsigned int||W||500|| LMM sending rate, in ms
#define Y1731_Lmm_Rate	"y1731.lmp.lmm.rate"

///|Y1731.{i}.Lmm.rmpid||Unsigned int||W||0||Identification of the destination MP instance
///:(1..8191)
///:0 --- used Y1731.{i}.Lmm.destmac only, the Y1731.{i}.Lmm.destmac cannot be empty
#define Y1731_Lmm_rmpid	"y1731.lmp.lmm.rmpid"

///|Y1731.{i}.Lmm.destmac||String||W|| ||	Mac address of the LMM destination in ‘XX:XX:XX:XX:XX:XX’ format.
#define Y1731_Lmm_destmac	"y1731.lmp.lmm.destmac"

///|Y1731.{i}.Lmm.timeout||Unsigned int||W||5000||timeout in mSecs, for optional timeout,on receiving responses
#define Y1731_Lmm_timeout	"y1731.lmp.lmm.timeout"

///|Y1731.{i}.Lmm.Status||String||R||||LMM operation status
///*Success: LMM is sent
///*Error: Cannot send LMM
#define Y1731_Lmm_Status	"y1731.lmp.lmm.status"



//send LMM message
//$  0 -- success
//$ <0 --- error code
int LMM_send(Session *pSession);

// finish sending LMM
int LMM_send_end(Session *pSession);

#endif
