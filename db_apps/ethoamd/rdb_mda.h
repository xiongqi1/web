#pragma once

///|Dot1ag.{1-4}.Mda.PeerMode||Bool||W||P||false||
///Multicast CFM Peer mode enable flag
///:false: '''disable Peer mode: disable 1ag functions'''
///:true: '''enable Peer mode: install 1ag,  and enable MEP. Reload modified parameters'''
///When 'Dot1ag.{1-4}.Mda.PeerMode' is set 'true', the following parameter will be reload if it has been changed
///*'Dot1ag.{1-4}.Mda.MdLevel',
///*'Dot1ag.{1-4}.Mda.PrimaryVid'
///*'Dot1ag.{1-4}.Mda.MDFormat'
///*'Dot1ag.{1-4}.Mda.MdIdType2or4'
///*'Dot1ag.{1-4}.Mda.MaFormat'
///*'Dot1ag.{1-4}.Mda.MaIdType2'
///*'Dot1ag.{1-4}.Mda.MaIdNonType2'
///*'Dot1ag.{1-4}.Mda.AVCID'
#define MDA_PeerMode	"dot1ag.mda.peermode"

///|Dot1ag.{1-4}.Mda.MdLevel||Unsigned int||W||P||2||MD level for this MDMA association (0..7)
#define MDA_MdLevel	"dot1ag.mda.mdlevel"

///|Dot1ag.{1-4}.Mda.PrimaryVid||Unsigned int||W||P||0|| MD primary CFI + VID, shared by all LMP.
///Value range:
///*0 - 4095: VID(0..4095), with CFI not set
///*4096-8191: VID(0..4095), with CFI is set
#define MDA_PrimaryVid	"dot1ag.mda.primaryvid"

///|Dot1ag.{1-4}.Mda.MDFormat||Unsigned int||W||P||1||MDID value range (1..4). its value:
///:1: None
///:2: DNS like
///:3: Max + 4 bytes (XX:XX:XX:XX:XX:XX (17 bytes) + XXXX (4 bytes))
///:4: String
#define MDA_MDFormat	"dot1ag.mda.mdformat"

///|Dot1ag.{1-4}.Mda.MdIdType2or4||String||W||P||MD1||MDID. This is in use when the Dot1ag.{1-4}.Mda.MDFormat is 2,3 or 4.
#define MDA_MdIdType2or4	"dot1ag.mda.mdidtype2or4"

///|Dot1ag.{1-4}.Mda.MaFormat||Unsigned int||W||P||2||MA format value range (1..4). its value:
///:1: Vid – 2 bytes,
///:2: String (up to 46 max)
///:3: Uint16  – 2 bytes
///:4: VPN id– 3 bytes
#define MDA_MaFormat	"dot1ag.mda.maformat"

///|Dot1ag.{1-4}.Mda.MaIdType2||String||W||P||MA1|| MAID. 46 char max string. Only in use when Dot1ag.{1-4}.Mda. MaFormat is 2.
#define MDA_MaIdType2	"dot1ag.mda.maidtype2"

///|Dot1ag.{1-4}.Mda.MaIdNonType2||Unsigned int||W||P||0||MAID. Only in use when Dot1ag.{1-4}.Mda. MaFormat is NOT 2.
#define MDA_MaIdNonType2	"dot1ag.mda.maidnontype2"

///|Dot1ag.{1-4}.Mda.CCMInterval||Unsigned int||W||P||4||CCM message time interval
///(0..7) default value is 4
///:0: disabled,
///:1: 3.3msec,
///:2: 10msec,
///:3: 100msec,
///:4: 1s,
///:5: 10s,
///:6: 1min,
///:7: 10min
#define MDA_CCMInterval	"dot1ag.mda.ccminterval"

///|Dot1ag.{1-4}.Mda.lowestAlarmPri||Unsigned int||W||P||2||An integer value specifying the lowest priority defect that is allowed to generate a Fault Alarm.
///(1..6) Default value is 2.
///:1: all
///:2: Only DefMACstatus, DefRemoteCCM, DefErrorCCM, and DefXconCCM
///:3: Only DefRemoteCCM, DefErrorCCM, and DefXconCCM
///:4: Only DefErrorCCM and DefXconCCM
///:5: Only DefXconCCM
///:6: No Alarm
#define MDA_lowestAlarmPri	"dot1ag.mda.lowestalarmpri"

///|Dot1ag.{1-4}.Mda.AVCID||String||W||P|| || 1AG attached AVC ID. 1AG is going to attach to the bridge interface for this AVC.
#define MDA_AVCID	"dot1ag.mda.avcid"

///|Dot1ag.{1-4}.Mda.GetStats||Bool||W|| ||false|| '''true -- refresh CCM, RMP stats data,'''
#define MDA_GetStats	"dot1ag.mda.getstats"

///|Dot1ag.{1-4}.Mda.errorCCMdefect||Bool||-|| ||false||CCM error received.
#define MDA_errorCCMdefect	"dot1ag.mda.errorccmdefect"

///|Dot1ag.{1-4}.Mda.xconCCMdefect||Bool||-|| ||false||Cross Connect CCM error received
#define MDA_xconCCMdefect	"dot1ag.mda.xconccmdefect"

///|Dot1ag.{1-4}.Mda.CCMsequenceErrors||Unsigned int||-|| ||0||CCM Sequence errors received
#define MDA_CCMsequenceErrors	"dot1ag.mda.ccmsequenceerrors"

///|Dot1ag.{1-4}.Mda.fngAlarmTime||Unsigned int||W||P||2500||The time that defects must be present before a Fault Alarm is issued in 10 msec unit:
/// (250..10000) Default is 2500.
#define MDA_fngAlarmTime	"dot1ag.mda.fngalarmtime"

///|Dot1ag.{1-4}.Mda.fngResetTime||Unsigned int||W||P||10000||The time that defects must be absent before resetting a Fault Alarm in 10 msec unit:
/// (250..10000) Default is 10000.
#define MDA_fngResetTime	"dot1ag.mda.fngresettime"

///|Dot1ag.{1-4}.Mda.someRMEPCCMdefect||Bool||-|| ||false||A Boolean indicating the aggregate state of the Remote MEP state machines. True indicates that at least one
///of the Remote MEP state machines is not receiving valid CCMs from its remote MEP, and false that all
///Remote MEP state machines are receiving valid CCMs. someRMEPCCMdefect is the logical OR of all of
///the rMEPCCMdefect variables for all of the Remote MEP state machines on this MEP. This variable is
///readable as a managed object [item q) in 12.14.7.1.3].
#define MDA_someRMEPCCMdefect	"dot1ag.mda.somermepccmdefect"

///|Dot1ag.{1-4}.Mda.someMACstatusDefect||Bool||-|| ||false||A Boolean indicating that one or more of the remote MEPs is reporting a failure in its Port Status TLV
///or Interface Status TLV . It is true if either some remote MEP is reporting that its interface
///is not isUp (i.e., at least one remote MEP’s interface is unavailable), or if all remote MEPs are reporting a
///Port Status TLV that contains some value other than psUp (i.e., all remote MEPs’ Bridge Ports are not
///forwarding data). 
#define MDA_someMACstatusDefect	"dot1ag.mda.somemacstatusdefect"

///|Dot1ag.{1-4}.Mda.someRDIdefect||Bool||-|| ||false||
///A Boolean indicating the aggregate health of the remote MEPs. True indicates that at least one of the
///Remote MEP state machines is receiving valid CCMs from its remote MEP that has the RDI bit set, and
///false that no Remote MEP state machines are receiving valid CCMs with the RDI bit se
#define MDA_someRDIdefect	"dot1ag.mda.somerdidefect"

///|Dot1ag.{1-4}.Mda.highestDefect||Unsigned int||-|| ||0||The highest priority defect that has been present since the MEPs Fault Notification
#define MDA_highestDefect	"dot1ag.mda.highestdefect"

///|Dot1ag.{1-4}.Mda.fngState||Unsigned int||-|| ||0||Indicates the different states of the MEP Fault Notification Generator State Machine
///:1: fngReset
///:2: fngDefect
///:3: fngReportDefect
///:4: fngDefectReported
///:5: fngDefectClearing
#define MDA_fngState	"dot1ag.mda.fngstate"

///|Dot1ag.{1-4}.Mda.Status||String||-|| ||"802.1ag is not started"||MDA status
///All error message related to 802.1ag setup
///*802.1ag is not installed --- ethoamd daemon is not started
///*802.1ag is not started --- ethoamd daemon is started, but the 802.1ag service is not enabled
///*Success: 802.1ag is ready
///*Success: 802.1ag is shutdown
///*Error: Incorrect MDA config
///*Error: Cannot find device:xxx
///*Error: Cannot bind to device:xxx
///*Error: Cannot register 802.1ag service
///*Error: Cannot open TMS device
#define MDA_Status	"dot1ag.mda.status"


/// this is intercommunication with mplsd module
/// when mplsd updates or deletes Bridge interface, it sends notice to ethoamd
#define MDA_Changed_IF	"dot1ag.mda.changedif"

/// when mplsd adds Bridge interface, it sends notice to ethoamd
#define MDA_Add_IF	"dot1ag.mda.addif"

/// attached interface
#define MDA_Attached_IF	"dot1ag.mda.attachedif"

#define	DOT1AG_Changed	"dot1ag.changed"

#define	DOT1AG_INDEX	"dot1ag._index"

#define	DOT1AG_TEST	"dot1ag.test"
#define	DOT1AG_TEST_MEP	"dot1ag.testmep"

#define DOT1AG_CMD_COMMAND  "dot1ag.cmd.command"
#define DOT1AG_CMD_PARAM_	"dot1ag.cmd.param."
#define DOT1AG_CMD_STATUS	"dot1ag.cmd.status"
#define DOT1AG_CMD_MESSAGE	"dot1ag.cmd.message"



//enable CCM
//$  0 -- success
//$ <0 --- error code
int CCM_enable(Session *pSession);

// disable CCM
int CCM_disable(Session *pSession);

//collect CCM
//$  0 -- success
//$ <0 --- error code
int CCM_collect(Session *pSession);

// auto adjust CCM state
//$  1 -- no change
//$  0 -- success, changed
//$ <0 --- error code
int CCM_auto_start(Session *pSession);
