#pragma once
#ifdef NCI_Y1731

///|Y1731.{1-4}.Slm.rmpid||Unsigned int||W||P||0||Identification of the destination MP instance
///:(0..8191)
///:0 --- used Y1731.{1-4}.Slm.destmac only, the Y1731.{1-4}.Slm.destmac cannot be empty
#define Y1731_Slm_rmpid	"y1731.lmp.slm.rmpid"

///|Y1731.{1-4}.Slm.destmac||String||W||P|| ||Mac address of the SLM destination in 'XX:XX:XX:XX:XX:XX' format.
#define Y1731_Slm_destmac	"y1731.lmp.slm.destmac"

///|Y1731.{1-4}.Slm.timeout||Unsigned int||W||P||5000||timeout in mSecs, value range: (0--4294967295)
/// for optional timeout,on receiving responses
#define Y1731_Slm_timeout	"y1731.lmp.slm.timeout"

///|Y1731.{1-4}.Slm.Send||Unsigned int||W|| ||0||'''number of SLM messages to send'''.  value range: (0..65536).
#define Y1731_Slm_Send	"y1731.lmp.slm.send"

///|Y1731.{1-4}.Slm.Rate||Unsigned int||W||P||500|| SLM sending rate, in ms.value range: (0--4294967295)
///If its value  is 1 - 10ms, it will be rounded to 10ms due to device driver timer resolution.
///If its value  is 0, it will send SLM as quick as possible.
#define Y1731_Slm_Rate	"y1731.lmp.slm.rate"

///|Y1731.{1-4}.Slm.TestID||String||W|| ||1|| SLM session Array of SLM Transaction Identifier, seperated by ",".
///The array maximum size is 20. 
///Each item is the test ID for one SLM session.
///For example: the value "10,20,30", once user trigger "Y1731.{1-4}.Slm.Send", it will start three SLM  session simultaneously.
///The test ID for each session is 10, 20, 30 .
#define Y1731_Slm_TestID	"y1731.lmp.slm.testid"

///|Y1731.{1-4}.Slm.TLVDataLen||Unsigned int||W||P||0||  Data TLV length (0..1460). 
#define Y1731_Slm_TLVDataLen	"y1731.lmp.slm.tlvdatalen"

///|Y1731.{1-4}.Slm.Curr||String||R|| || ||Session array of current loss value. seperated by ",". format: near_end_loss:far_end_loss. 
///The array maximum size is 20. 
#define Y1731_Slm_Curr	"y1731.lmp.slm.curr"

///|Y1731.{1-4}.Slm.Accum||String||R|| || ||Session array of accumulated loss value. seperated by ",".
///Its maximum size is 20.
///Each item format: near_end_loss:far_end_loss. 
///The accumulated loss value pairs are calculated on each SLR  by
///*near_end_loss = Y1731.{1-4}.Slm.Accum.near_end_loss  +  Y1731.{1-4}.Slm.Curr.near_end_loss
///*far_end_loss  = Y1731.{1-4}.Slm.Accum.far_end_loss  + Y1731.{1-4}.Slm.Curr.far_end_loss
#define Y1731_Slm_Accum	"y1731.lmp.slm.accum"

///|Y1731.{1-4}.Slm.Ratio||String||R|| || || Session array of the ratio of loss value. seperated by ","
///Its maximum size is 20.
///Each item format: near_end_loss:far_end_loss. 
///The ratio pairs are calculated on each SLR  by 
///*near_end_loss = Y1731.{1-4}.Slm.Accum.near_end_loss*100 /SLR.txfcb
///*far_end_loss = Y1731.{1-4}.Slm.Accum.far_end_loss*100 / SLR.txfcf.
///So they are  the unsigned integer value whose last two digits are in decimal.
///For example:  in one test, the "y1731.1.lmp.slm.ratio  =   19228:0" , 
///which  means ratio of near_end_loss   192.28 and ratio of  far_end_loss is 0
#define Y1731_Slm_Ratio	"y1731.lmp.slm.ratio"

///|Y1731.{1-4}.Slm.Count||String||R|| || ||Session array of number of SLR received, seperated by ","
///Its maximum size is 20. Each item format is in integer.
#define Y1731_Slm_Count	"y1731.lmp.slm.count"

///|Y1731.{1-4}.Slm.Status||String||R|| || ||SLM operation status
///*Error: 802.1ag is not started
///*Error: Y.1731 is not enabled -- SLM need Y.1731 function enabled, (Y1731.{1-4}.Mda.Enable is true)
///*Error: Cannot send SLM -- internal error, cannot start  the sending process
///*Success: SLM is sent --- test session is in processing and waiting tor finish
///*Failed: SLM has no response -- test session is finished, but there is no any reponse from far end
///*Error: in sending SLM. --- internal error, the sending process is broken.
///*Success: SLM is completed -- test session is finished, there is one or more reponse from far end.
///*Error: Cannot get Y.1731 stats -- internal error, failed to invoke some APIs to retrieve stats from device driver
#define Y1731_Slm_Status	"y1731.lmp.slm.status"


//send SLM message
//$  0 -- success
//$ <0 --- error code
int SLM_send(Session *pSession);

// finish sending SLM
int SLM_send_end(Session *pSession);


#endif
