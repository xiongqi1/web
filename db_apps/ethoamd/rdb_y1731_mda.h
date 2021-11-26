#pragma once

#include "rdb_mda.h"

#ifdef NCI_Y1731





///|Y1731.{1-4}.Mda.Enable||Bool||W||P|| false||Y.1731 protocol enable flag.
///:true --- '''Y.1731 service is enabled'''
///:false -- '''Y.1731 service is disabled'''
///Note: Dot1ag should be enabled first (i.e. peermode = true ) to make Y.1731 service working.
#define Y1731_Mda_Enable	"y1731.mda.enable"

///|Y1731.{1-4}.Mda.AVCID||String||W||P|| || 1AG/Y.1731 attached AVC ID. 1AG/Y.1731 is going to attach to the bridge interface for this AVC.
///* alias to "Dot1ag.{1-4}.Mda.AVCID"
#define A_Y1731_Mda_AVCID	MDA_AVCID

///|Y1731.{1-4}.Mda.MegLevel||Unsigned int||W||P|| 2||MEG level for this MEGID association (0..7)
#define Y1731_Mda_MegLevel	"y1731.mda.meglevel"

///|Y1731.{1-4}.Mda.MegId||String||W||P||||Y.1731 MEGID.
#define Y1731_Mda_MegId	"y1731.mda.megid"

///|Y1731.{1-4}.Mda.MegIdFormat||Unsigned int||W||P||32||Y.1731 MEGID format. value range (1..32). Only the 1,2,3,4 and 32 value are used.
#define Y1731_Mda_MegIdFormat	"y1731.mda.megidformat"

///|Y1731.{1-4}.Mda.MegIdLength||Unsigned int||W||P||13||Y.1731 MEGID length. value range (0..45)
#define Y1731_Mda_MegIdLength	"y1731.mda.megidlength"

///|Y1731.{1-4}.Mda.someRDIdefect||Unsigned int||R||||0||The logical OR of all of the rMEPlastRDI variables for all of the Remote MEP state machines on this MEP.
///* alias to "Dot1ag.{1-4}.Mda.someRDIdefect"
#define A_Y1731_Mda_someRDIdefect	MDA_someRDIdefect

///|Y1731.{1-4}.Mda.ALMSuppressed||Bool||R|| ||false||Whether alarm has been suppressed
#define Y1731_Mda_ALMSuppressed	"y1731.mda.almsuppressed"

///|Y1731.{1-4}.Mda.Status||String||R|| || ||MDA status.
///* alias to "Dot1ag.{1-4}.Mda.Status"
///All error message related to 802.1ag setup
///*802.1ag is not installed --- ethoamd daemon is not started
///*802.1ag is not started --- ethoamd daemon is started, but the 802.1ag service is not enabled
///*Success: 802.1ag is ready
///*Success: 802.1ag/Y.1731 is ready
///*Success: 802.1ag is shutdown
///*Error: Incorrect MDA config
///*Error: Cannot find device:xxx
///*Error: Cannot bind to device:xxx
///*Error: Cannot register 802.1ag service
///*Error: Cannot open TMS device
#define A_Y1731_Mda_Status	MDA_Status


#endif
