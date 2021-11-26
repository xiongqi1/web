
///|Dot1ag.{1-4}.Rmp.{j}.mpid||Unsigned int||W||P||0||RMPID, Identification of this RMP instance
///:(0..8191)
///:0 --- disable this RMP
#define RMP_mpid	"dot1ag.rmp.mpid"

///|Dot1ag.{1-4}.Rmp.{j}.ccmDefect||Bool||-|| ||false||CCM state of this RMP instance:
///:false: no CCM from the RMP for the ‘3 * CCMInterval’ sec.
///:true: OK
#define RMP_ccmDefect	"dot1ag.rmp.ccmdefect"

///|Dot1ag.{1-4}.Rmp.{j}.lastccmRDI||Bool||-|| || false||last RDI flag from the last-received CCM of this RMP instance
#define RMP_lastccmRDI	"dot1ag.rmp.lastccmrdi"

///|Dot1ag.{1-4}.Rmp.{j}.lastccmPortState||Unsigned int||-|| ||0||last port state from the last-received CCM of this RMP instance
#define RMP_lastccmPortState	"dot1ag.rmp.lastccmportstate"

///|Dot1ag.{1-4}.Rmp.{j}.lastccmIFStatus||Unsigned int||-|| ||0||last Interface state from the last-received CCM of this RMP instance
#define RMP_lastccmIFStatus	"dot1ag.rmp.lastccmifstatus"

///|Dot1ag.{1-4}.Rmp.{j}.lastccmSenderID||Unsigned int||-|| ||0||last Sender ID from the last-received CCM of this RMP instance
#define RMP_lastccmSenderID	"dot1ag.rmp.lastccmsenderid"

///|Dot1ag.{1-4}.Rmp.{j}.lastccmmacAddr||String||-|| || ||last-received CCM’s source address for this RMP instance in ‘XX:XX:XX:XX:XX:XX’ format.
#define RMP_lastccmmacAddr	"dot1ag.rmp.lastccmmacaddr"

///|Dot1ag.{1-4}.Rmp.{j}.TxCounter||Unsigned int||R|| ||0||# of Total CFM packets sent from this RMP 
#define RMP_TxCounter	"dot1ag.rmp.txcounter"

///|Dot1ag.{1-4}.Rmp.{j}.RxCounter||Unsigned int||R|| ||0||# of Total CFM packets received on this RMP 
#define RMP_RxCounter	"dot1ag.rmp.rxcounter"


///|Dot1ag.{1-4}.Rmp.{j}.Curr||String||R|| || || Current CCM loss value for this RMPID. format: near_end_loss:far_end_loss. 
///report only when Y1731 is enabled(Y1731.{1-4}.Mda.Enable is true)
#define RMP_Curr	"dot1ag.rmp.curr"

///|Dot1ag.{1-4}.Rmp.{j}.Accum||String||R|| || || Accumulated CCM loss value for this RMPID. format: near_end_loss:far_end_loss. 
///report only when Y1731 is enabled(Y1731.{1-4}.Mda.Enable is true)
///The accumulated loss value pairs are calculated on each CCM  by
///*near_end_loss = Dot1ag.{1-4}.Rmp.{j}.Accum.near_end_loss  +  Dot1ag.{1-4}.Rmp.{j}.Curr.near_end_loss
///*far_end_loss  = Dot1ag.{1-4}.Rmp.{j}.Accum.far_end_loss  + Dot1ag.{1-4}.Rmp.{j}.Curr.far_end_loss
#define RMP_Accum	"dot1ag.rmp.accum"

///|Dot1ag.{1-4}.Rmp.{j}.Ratio||String||R|| || || The ratio of CCM  loss value for this RMPID. format: near_end_loss:far_end_loss. 
///report only when Y1731 is enabled(Y1731.{1-4}.Mda.Enable is true)
///The ratio pairs are calculated on each CCM  by 
///*near_end_loss = Dot1ag.{1-4}.Rmp.{j}.Accum.near_end_loss*100 /CCM.txfcb
///*far_end_loss = Dot1ag.{1-4}.Rmp.{j}.Accum.far_end_loss*100 / CCM.txfcf.
///so they are  the unsigned integer value whose last two digits are in decimal.
#define RMP_Ratio	"dot1ag.rmp.ratio"


/// current RMP object id list
/// sperateted by ','
#define	RMP_index		"dot1ag.rmp._index"


//#define	RMP_status		"dot1ag.rmp.status"

#include "session.h"


// allocate RMP slot if it does not exit,
// 1) alloc memory block
// 2) create rdb
//$  0 -- success
//$ <0 --- error code
int RMP_add(Session *pSession,  int objectid);

// delete one slot of rmpid
// 1) disabled it
// 2) delete all rdb
// 3) release memory block
// update_index -- weather update index immedietly
//$  0 -- success
//$ <0 --- error code
int RMP_del(Session *pSession, int objectid, int update_index);

// search rmp list for object id
int RMP_find(Session *pSession,  int rmpid);

// assign a object with new RMPID
// 1) make sure it do exist
// 2) set rmpid into rdb
// 3) enable rdb or not
//$  0 -- success
//$ <0 --- error code
int RMP_set(Session *pSession, int objectid, int rmpid, int enable);

// enable one RMP object
// read rmpid from rdb
// RMPID changed!. if not same and enabled, disabeld it
// if not enable, enable it
//$  0 -- success
//$ <0 --- error code

int RMP_enable(Session *pSession, int objectid);

// collection all enabled RMP data into RDB
int RMP_collect(Session *pSession);

// disable one RMP, delete rdb variable
//$  0 -- success
//$ <0 --- error code
int  RMP_disable(Session *pSession, int objectid);


// enable all RMP, delete rdb variable
void RMP_enable_all(Session *pSession);

// disable one RMP, delete rdb variable
void RMP_disable_all(Session *pSession);

// delete all rmp slot and its rdb
void RMP_del_all(Session *pSession);

#ifdef _RDB_RPC
// send rmp list by RPC
void RMP_rpc_list(Session *pSession);
#endif

// retrieve RMP rdb data into memory
void RMP_retrieve(Session *pSession);

// sync from RDB object, build RMP object
// 1) rdb exist, RMP not exist -- create RMP for it
// 2) rdb  not exit, RMP exist -- delete RMP
// 3) rdb exist, RMP exist     -- sync RDB to RMP, readonly object
void RMP_update(Session *pSession);
