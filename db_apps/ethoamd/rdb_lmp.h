#pragma once

///|Dot1ag.{1-4}.Lmp.MEPactive||Bool||W||P||false||LMP Active flag
///:false: '''disable'''
///:true: '''enable MEP mode, reload LMP parameter'''
///When 'Dot1ag.{1-4}.Lmp.MEPactive' is set to true, the following parameter will be reload if it has any modification
///*'Dot1ag.{1-4}.Lmp.mpid'
///*'Dot1ag.{1-4}.Lmp.direction'
///*'Dot1ag.{1-4}.Lmp.port'
///*'Dot1ag.{1-4}.Lmp.vid'
///*'Dot1ag.{1-4}.Lmp.vidtype'
///*'Dot1ag.{1-4}.Lmp.CoS'
///Indicate whether the MEP mode is active, it is also controlled by Dot1ag.{1-4}.Lmp.PeerMode
#define LMP_MEPactive	"dot1ag.lmp.mepactive"

///|Dot1ag.{1-4}.Lmp.mpid||Unsigned int||W||P||1||Identification of this LMP instance
///(0..8191)
#define LMP_mpid	"dot1ag.lmp.mpid"

///|Dot1ag.{1-4}.Lmp.direction||int||W||P||1||Direction of this LMP instance (-1..2)
///:1: Up
///:-1 or 2: Down
///:0: MIP
#define LMP_direction	"dot1ag.lmp.direction"

///|Dot1ag.{1-4}.Lmp.port||Unsigned int||W||P||2||bridge Port of this LMP instance. it only affected sending LTM,LBM,DMM, LMM from WNTD
///:1 -- send packet through UNID port in bridge interface, 
///:2 -- send packet through GRE port in bridge interface, 
///:0 or other value -- send packet on bridge interface, the bridge interface will decide which port to send.
///Since the bridge interface has learning machanism, The LBM or DMM  packet may be sent to both ports initially.
///The LTM packet is always sent on both ports.
#define LMP_port	"dot1ag.lmp.port"

///|Dot1ag.{1-4}.Lmp.vid||Unsigned int||W||P||0|| CFI+VID of this LMP instance, if it is 0, WNTD may use the value from 'Dot1ag.{1-4}.Mda.PrimaryVid'
///Value range:
///*'''0 - 4095: VID(0..4095), with CFI not set'''
///*'''4096-8191: VID(0..4095), with CFI is set'''
#define LMP_vid	"dot1ag.lmp.vid"

///|Dot1ag.{1-4}.Lmp.vidtype||Unsigned int||W||P||0||VID type of this LMP instance.
///:(0..2)
///:'''0: no VLAN'''
///:'''1: CVLAN, 802.1Q'''
///:'''2: SVLAN, 802.1AD'''
///In  CVLAN or SVLAN mode, the  VID value comes from 'Dot1ag.{1-4}.Lmp.vid' or 'Dot1ag.{1-4}.Mda.PrimaryVid' if the previous one is 0
#define LMP_vidtype	"dot1ag.lmp.vidtype"

///|Dot1ag.{1-4}.Lmp.macAdr||String||R||P|| ||Local mac address of this LMP instance in ‘XX:XX:XX:XX:XX:XX’ format.
///it is got from attached interface, eg: br1.
#define LMP_macAdr	"dot1ag.lmp.macadr"

///|Dot1ag.{1-4}.Lmp.CCMenable||Bool||W||P||false||CCM enable flag
///:false: '''disable'''
///:true: '''enable sending CCM message'''
#define LMP_CCMenable	"dot1ag.lmp.ccmenable"

///|Dot1ag.{1-4}.Lmp.CCIsentCCMs||Unsigned int||R|| ||1||CCM sequense number, it automatically increases for each sent CCM. 
///restart when LMP is reset.
#define LMP_CCIsentCCMs	"dot1ag.lmp.ccisentccms"

///|Dot1ag.{1-4}.Lmp.xconnCCMdefect||Unsigned int||-|| ||0||# of CCM cross-connect defects received.
#define LMP_xconnCCMdefect	"dot1ag.lmp.xconnccmdefect"

///|Dot1ag.{1-4}.Lmp.errorCCMdefect||Unsigned int||-|| ||0||# of CCM defects received.
#define LMP_errorCCMdefect	"dot1ag.lmp.errorccmdefect"

///|Dot1ag.{1-4}.Lmp.CCMsequenceErrors||Unsigned int||-|| ||0||# of Sequence CCM defects received.
#define LMP_CCMsequenceErrors	"dot1ag.lmp.ccmsequenceerrors"

///|Dot1ag.{1-4}.Lmp.TxCounter||Unsigned int||R|| ||0||# of Total CFM packets sent from this LMP 
#define LMP_TxCounter	"dot1ag.lmp.txcounter"

///|Dot1ag.{1-4}.Lmp.RxCounter||Unsigned int||R|| ||0||# of Total CFM packets received on this LMP 
#define LMP_RxCounter	"dot1ag.lmp.rxcounter"

///|Dot1ag.{1-4}.Lmp.Status||String||-|| || ||status related to  LMP,  MEP or CCM operations
///*Success: LMP is enabled
///*Success: LMP is disabled
///*Success: MEP is active
///*Success: MEP is inactive
///*Success: CCM is enabled
///*Success: CCM is disabled
///*Success: collect CCM stats
///*Error: 802.1ag is not started
///*Error: Cannot enable MEP
///*Error: Cannot disable MEP
///*Error: cannot setup LMP end point xxx
///*Error: Cannot enable CCM
///*Error: Cannot disable CCM
///*Error: Cannot collect CCM stats
#define LMP_Status	"dot1ag.lmp.status"

///|Dot1ag.{1-4}.Lmp.CoS||Unsigned int||W||P||0||CoS value.(0..7) It replaces the priority field in LBM/LTM
///:0: TC4
///:5: TC1
#define LMP_CoS	"dot1ag.lmp.cos"

///|Dot1ag.{1-4}.Lmp.LBRsInOrder||Unsigned int||-|| ||0||# of correct (i.e. in order) LBR messages received
#define LMP_LBRsInOrder	"dot1ag.lmp.lbrsinorder"

///|Dot1ag.{1-4}.Lmp.LBRsOutOfOrder||Unsigned int||-|| ||0||# of incorrect (i.e. out of order) LBR messages received
#define LMP_LBRsOutOfOrder	"dot1ag.lmp.lbrsoutoforder"

///|Dot1ag.{1-4}.Lmp.LBRnoMatch||Unsigned int||-|| ||0||# of not matched (i.e. not existed in the LBM sent list) LBR messages received
#define LMP_LBRnoMatch	"dot1ag.lmp.lbrnomatch"

///|Dot1ag.{1-4}.Lmp.LBRsTransmitted||Unsigned int||-|| || 0||# of LBR messages sent as a response to the LBM received from a remote MEP.
#define LMP_LBRsTransmitted	"dot1ag.lmp.lbrstransmitted"

///|Dot1ag.{1-4}.Lmp.LTRsUnexpected||Unsigned int||-|| || 0||# of the unexpected LTRs received
#define LMP_LTRsUnexpected	"dot1ag.lmp.ltrsunexpected"


#include "session.h"



//enable LMP
//$  0 -- success
//$ <0 --- error code
int LMP_enable(Session *pSession);

//disable LMP
//$  0 -- success
//$ <0 --- error code
int LMP_disable(Session *pSession);

//sync  LMP_Enable state
//$  0 -- success
//$ <0 --- error code
//int LMP_sync(Session *pSession);

// check whether lmp config changed
// >0 -- changed
// 0 -- not changed
int check_lmp_config(Session *pSession);

//enable LMP
//$  0 -- success
//$ <0 --- error code
int MEP_enable(Session *pSession);

//disable LMP
//$  0 -- success
//$ <0 --- error code
int MEP_disable(Session *pSession);

//auto adjust MEP state
//$  1 -- no change
//$  0 -- success changed
//$ <0 --- error code
int MEP_auto_start(Session *pSession);

//enable MEP check RMP
//$  0 -- success
//$ <0 --- error code
int MEP_RMP_Check(Session *pSession, unsigned int rmpid, MACADR	destmac, char *lasterr);


// check whether any mda parameter, if changed ,reload
// >0 -- changed
// 0 -- not changed
int install_extra_mda_config(Session *pSession);

//collect all stats on this MEP
//$  0 -- success
//$ <0 --- error code
int MEP_collect(Session *pSession);
