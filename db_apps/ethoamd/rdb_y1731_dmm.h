#pragma once
#ifdef NCI_Y1731



///|Y1731.{1-4}.Dmm.rmpid||Unsigned int||W||P||0||Identification of the destination MP instance
///:(0..8191)
///:0 --- used Y1731.{1-4}.Dmm.destmac only, the Y1731.{1-4}.Dmm.destmac cannot be empty
#define Y1731_Dmm_rmpid	"y1731.lmp.dmm.rmpid"

///|Y1731.{1-4}.Dmm.destmac||String||W||P|| ||Mac address of the DMM/DM1 destination in 'XX:XX:XX:XX:XX:XX' format.
#define Y1731_Dmm_destmac	"y1731.lmp.dmm.destmac"

///|Y1731.{1-4}.Dmm.timeout||Unsigned int||W||P||5000||timeout in mSecs, for optional timeout,on receiving responses
///value range: (0--4294967295)
#define Y1731_Dmm_timeout	"y1731.lmp.dmm.timeout"

///|Y1731.{1-4}.Dmm.Send||Unsigned int||W|| ||0||'''number of DMM/DM1 messages to send'''.  value range: (0..65536).
#define Y1731_Dmm_Send	"y1731.lmp.dmm.send"

///|Y1731.{1-4}.Dmm.Rate||Unsigned int||W||P||500|| DMM or DM1 sending rate, in ms. value range: (0--4294967295)
///If its value  is 1 - 10ms, it will be rounded to 10ms due to device driver timer resolution.
///If its value  is 0, it will send DMM as quick as possible.
#define Y1731_Dmm_Rate	"y1731.lmp.dmm.rate"

///|Y1731.{1-4}.Dmm.Type||Unsigned int||W||P||0||  DMM type (0..1)
///:0 --- DMM 
///:1 --- DM1
#define Y1731_Dmm_Type	"y1731.lmp.dmm.type"

///|Y1731.{1-4}.Dmm.TLVDataLen||Unsigned int||W||P||0||  Data TLV length (0..1446). 
///The maximum length "1446" is valid only when "unid.max_frame_size >= 1522. 
///if unid.max_frame_size reduces, the maximum "1446" will be reduced in same amount.
#define Y1731_Dmm_TLVDataLen	"y1731.lmp.dmm.tlvdatalen"

///|Y1731.{1-4}.Dmm.Dly||String||R|| || || last Delay value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_Dly	"y1731.lmp.dmm.dly"

///|Y1731.{1-4}.Dmm.DlyAvg||String||R|| || || the average Delay value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_DlyAvg	"y1731.lmp.dmm.dlyavg"

///|Y1731.{1-4}.Dmm.DlyMin||String||R|| || ||the minimium Delay value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_DlyMin	"y1731.lmp.dmm.dlymin"

///|Y1731.{1-4}.Dmm.DlyMax||String||R|| || ||the maximium Delay value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_DlyMax	"y1731.lmp.dmm.dlymax"

///|Y1731.{1-4}.Dmm.Var||String||R|| || ||last Variation value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_Var	"y1731.lmp.dmm.var"

///|Y1731.{1-4}.Dmm.VarAvg||String||R|| || ||the average Variation value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_VarAvg	"y1731.lmp.dmm.varavg"

///|Y1731.{1-4}.Dmm.VarMin||String||R|| || ||the minimium Variation value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_VarMin	"y1731.lmp.dmm.varmin"

///|Y1731.{1-4}.Dmm.VarMax||String||R|| || ||the maximium Variation value, format is in decimal of millisecond. "mmm.uuu"
#define Y1731_Dmm_VarMax	"y1731.lmp.dmm.varmax"

///|Y1731.{1-4}.Dmm.Count||Unsigned int||R|| ||0||number of  DMR received
#define Y1731_Dmm_Count	"y1731.lmp.dmm.count"

///|Y1731.{1-4}.Dmm.Status||String||R|| || ||DMM operation status
///*Error: 802.1ag is not started
///*Error: Y.1731 is not enabled
///*Error: Cannot send DMM
///*Success: DMM is sent
///*Failed: DMM has no response
///*Error: in sending DMM
///*Success: DMM is completed
///*Error: Cannot get Y.1731 stats
///*Error: Cannot send DM1
///*Success: DM1 is sent
///*Error: in sending DM1
///*Success: DM1 is completed
#define Y1731_Dmm_Status	"y1731.lmp.dmm.status"




//send DMM message
//$  0 -- success
//$ <0 --- error code
int DMM_send(Session *pSession);

// finish sending DMM
int DMM_send_end(Session *pSession);


#endif
