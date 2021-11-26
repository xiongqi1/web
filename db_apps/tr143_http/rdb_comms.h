/* ----------------------------------------------------------------------------
RDB interface program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_COMMS_H
#define __RDB_COMMS_H


#include "rdb_ops.h"


/// download rdb variable

///Diagnostics.Capabilities.DownloadTransports 	string 	readonly 	none 	HTTP 		Supported transport protocols for TR-143 Download:1 profile.
//#define Diagnostics_Capabilities_DownloadTransports "capabilities.downloadtransports"

///Diagnostics.HttpMovingAverageWindowSize  uint readwrite none 4  moving avarage window size
#define Diagnostics_HttpMovingAverageWindowSize "diagnostics.httpmovingaveragewindowsize"

#define Default_HttpMovingAverageWindowSize 5

///Diagnostics.Download. 	object 	readonly 	none 	present 		DownloadPlus diagnostic object.
#define Diagnostics_Download "download."

///Diagnostics.Download.DiagnosticsState 	string 	readwrite 	none 	None
///Upload diagnostic subsystem state.
///    * "None" - service is idle waiting for a request.
///   * "Requested" - set by ACS after configuring test to request CPE to begin the test.
///    * "Completed" - set by CPE to indicate successful test completion.
///    * "Error_<description>" - set by CPE to indicate failure of test.
#define Diagnostics_Download_DiagnosticsState "download.%i.diagnosticsstate"

/// force to reload all parameters
#define Diagnostics_Download_Changed "download.%i.changed"

///Diagnostics.Download.StartTest	int 	readwrite 	none 	0	Start Console download test.
#define Diagnostics_Download_StartTest	"download.%i.starttest"

///Diagnostics.Download.SmartEdgeAddress 	string 	readwrite 	none 	0.0.0.0 		IP address of the CPG to establish test AVC to.
#define Diagnostics_Download_SmartEdgeAddress  "download.%i.smartedgeaddress"

///Diagnostics.Download.MPLSTag 	uint 	readwrite 	none 	0 		MPLS label for the test AVC.
#define Diagnostics_Download_MPLSTag  "download.%i.mplstag"

///Diagnostics.Download.CoS 	uint 	readwrite 	none 	0 		CoS for the test traffic.
#define Diagnostics_Download_CoS  "download.%i.cos"

///Diagnostics.Download.CoStoEXP 	string 	readwrite 	none 	0,1,2,3,4,5,6,7 		CoS to MPLS experimental bits mapping for the test AVC (only the utilised CoS value mapping need be non-zero).
#define Diagnostics_Download_CoStoEXP  "download.%i.costoexp"

///Diagnostics.Download.CoStoDSCP 	string 	readwrite 	none 	0,0,0,16,32,40,0,0 		CoS to IP-GRE DSCP mapping for the test AVC (only the utilised CoS value mapping need be non-zero).
#define Diagnostics_Download_CoStoDSCP  "download.%i.costodscp"

///Diagnostics.Download.InterfaceAddress 	string 	readwrite 	none 	0.0.0.0 		IPv4 address to assign to test interface.
#define Diagnostics_Download_InterfaceAddress  "download.%i.interfaceaddress"

///Diagnostics.Download.InterfaceNetmask 	string 	readwrite 	none 	0.0.0.0 		IPv4 address mask to assign to test interface.
#define Diagnostics_Download_InterfaceNetmask  "download.%i.interfacenetmask"

///Diagnostics.Download.DownloadURL 	string 	readwrite 	none 			URL to download for the test. Host part must be resolvable by the WNTD DNS configuration or an IP address specified. HTTP server must be reachable via the test interface.
#define Diagnostics_Download_DownloadURL "download.%i.downloadurl"

///Diagnostics.Download.ROMTime 	datetime 	readonly 	none 			Request time.
#define Diagnostics_Download_ROMTime "download.%i.romtime"
///Diagnostics.Download.BOMTime 	datetime 	readonly 	none 			Beginning of download time.
#define Diagnostics_Download_BOMTime "download.%i.bomtime"
///Diagnostics.Download.EOMTime 	datetime 	readonly 	none 			Completion of download time.
#define Diagnostics_Download_EOMTime "download.%i.eomtime"
///Diagnostics.Download.TestBytesReceived 	uint 	readonly 	none 	0 		Size of the file received.
#define Diagnostics_Download_TestBytesReceived "download.%i.testbytesreceived"
///Diagnostics.Download.TotalBytesReceived 	uint 	readonly 	none 	0 		Bytes received over interface during download.
#define Diagnostics_Download_TotalBytesReceived "download.%i.totalbytesreceived"


///Diagnostics.Download.SampleInterval	uint 	readwrite	none 	5 		interval(second)  for each sample
//#define Diagnostics_Download_SampleInterval "download.%i.sampleinterval"

///Diagnostics.Download.MinThroughput	uint 	readonly	none 	0 		Minimum throughput  to download	in one sample
#define Diagnostics_Download_MinThroughput	"download.%i.minthroughput"

///Diagnostics.Download.MaxThroughput	uint 	readonly	none 	0 		Maximum	throughout to download	in one sample
#define Diagnostics_Download_MaxThroughput	"download.%i.maxthroughput"

///Diagnostics.Download.Throughput	uint 	readonly	none 	0 		Maximum	throughout to download	in one sample
#define Diagnostics_Download_Throughput	"download.%i.throughput"

///Diagnostics.Download.TC1_CIR		uint 	readonly	none 	1500000 		TC1_CIR  rate
#define Diagnostics_Download_TC1_CIR	"download.%i.tc1_cir"

// TC2_PIR rate
#define Diagnostics_Download_TC2_PIR    "download.%i.tc2_pir"

///Diagnostics.Download.TC4_PIR		uint 	readonly	none 	10000000 		TC4_PIR  rate
#define Diagnostics_Download_TC4_PIR	"download.%i.tc4_pir"


/// upload rdb variable

///Diagnostics.Capabilities.UploadTransports 	string 	readonly 	none 	HTTP 		Supported transport protocols for TR-143 Upload:1 profile.
//#define Diagnostics_Capabilities_UploadTransports "capabilities.uploadtransports"

///Diagnostics.Upload. 	object 	readonly 	none 	present 		UploadPlus diagnostic object.
#define Diagnostics_Upload "upload."

///Diagnostics.Upload.DiagnosticsState 	string 	readwrite 	none 	None
///Upload diagnostic subsystem state.
///    * "None" - service is idle waiting for a request.
///   * "Requested" - set by ACS after configuring test to request CPE to begin the test.
///    * "Completed" - set by CPE to indicate successful test completion.
///    * "Error_<description>" - set by CPE to indicate failure of test.
#define Diagnostics_Upload_DiagnosticsState "upload.%i.diagnosticsstate"

/// force to reload all parameters
#define Diagnostics_Upload_Changed "upload.%i.changed"

///Diagnostics.Upload.StartTest	int 	readwrite 	none 	0	Start Console upload test.
#define Diagnostics_Upload_StartTest	"upload.%i.starttest"

///Diagnostics.Upload.SmartEdgeAddress 	string 	readwrite 	none 	0.0.0.0 		IP address of the CPG to establish test AVC to.
#define Diagnostics_Upload_SmartEdgeAddress  "upload.%i.smartedgeaddress"

///Diagnostics.Upload.MPLSTag 	uint 	readwrite 	none 	0 		MPLS label for the test AVC.
#define Diagnostics_Upload_MPLSTag  "upload.%i.mplstag"

///Diagnostics.Upload.CoS 	uint 	readwrite 	none 	0 		CoS for the test traffic.
#define Diagnostics_Upload_CoS  "upload.%i.cos"

///Diagnostics.Upload.CoStoEXP 	string 	readwrite 	none 	0,1,2,3,4,5,6,7 		CoS to MPLS experimental bits mapping for the test AVC (only the utilised CoS value mapping need be non-zero).
#define Diagnostics_Upload_CoStoEXP  "upload.%i.costoexp"

///Diagnostics.Upload.CoStoDSCP 	string 	readwrite 	none 	0,0,0,16,32,40,0,0 		CoS to IP-GRE DSCP mapping for the test AVC (only the utilised CoS value mapping need be non-zero).
#define Diagnostics_Upload_CoStoDSCP  "upload.%i.costodscp"

///Diagnostics.Upload.InterfaceAddress 	string 	readwrite 	none 	0.0.0.0 		IPv4 address to assign to test interface.
#define Diagnostics_Upload_InterfaceAddress  "upload.%i.interfaceaddress"

///Diagnostics.Upload.InterfaceNetmask 	string 	readwrite 	none 	0.0.0.0 		IPv4 address mask to assign to test interface.
#define Diagnostics_Upload_InterfaceNetmask  "upload.%i.interfacenetmask"

///Diagnostics.Upload.UploadURL 	string 	readwrite 	none 			URL to upload for the test. Host part must be resolvable by the WNTD DNS configuration or an IP address specified. HTTP server must be reachable via the test interface.
#define Diagnostics_Upload_UploadURL "upload.%i.uploadurl"

///Diagnostics.Upload.TestFileLength 	uint 	readwrite 	none 	0 		Size of test file to generate in bytes.
#define Diagnostics_Upload_TestFileLength "upload.%i.testfilelength"

///Diagnostics.Upload.ROMTime 	datetime 	readonly 	none 			Request time.
#define Diagnostics_Upload_ROMTime "upload.%i.romtime"

///Diagnostics.Upload.BOMTime 	datetime 	readonly 	none 			Beginning of upload time.
#define Diagnostics_Upload_BOMTime "upload.%i.bomtime"

///Diagnostics.Upload.EOMTime 	datetime 	readonly 	none 			Completion of upload time.
#define Diagnostics_Upload_EOMTime "upload.%i.eomtime"

///Diagnostics.Upload.TotalBytesSent 	uint 	readonly 	none 	0 		Bytes sent into interface during upload.
#define Diagnostics_Upload_TotalBytesSent "upload.%i.totalbytessent"

///Diagnostics.Upload.SampleInterval	uint 	readwrite	none 	5 		interval(second)  for each sample
//#define Diagnostics_Upload_SampleInterval "upload.%i.sampleinterval"

///Diagnostics.Upload.MinThroughput	uint 	readonly	none 	0 		Minimum throughput to upload	in one sample
#define Diagnostics_Upload_MinThroughput	"upload.%i.minthroughput"

///Diagnostics.Upload.MaxThroughput	uint 	readonly	none 	0 		Maximum	throughput to upload	in one sample
#define Diagnostics_Upload_MaxThroughput	"upload.%i.maxthroughput"

///Diagnostics.Upload.Throughput	uint 	readonly	none 	0 		Maxmium	throughput to upload	in one sample
#define Diagnostics_Upload_Throughput	"upload.%i.throughput"

///Diagnostics.Upload.TC1_CIR		uint 	readonly	none 	1500000 		TC1_CIR  rate
#define Diagnostics_Upload_TC1_CIR	"upload.%i.tc1_cir"

///Diagnostics.Upload.TC4_PIR		uint 	readonly	none 	10000000 		TC4_PIR  rate
#define Diagnostics_Upload_TC4_PIR	"upload.%i.tc4_pir"

// TC2_PIR rate
#define Diagnostics_Upload_TC2_PIR      "upload.%i.tc2_pir"


#define RDB_VAR_NONE 			0
#define RDB_VAR_DOWNLOAD_IF		0x01 // used by IF interface
#define RDB_VAR_UPLOAD_IF		0x02 // used by IF interface
#define RDB_VAR_DOWNLOAD_TEST_IF	0x04 // rdb used only by test console
#define RDB_VAR_UPLOAD_TEST_IF		0x08 // rdb used only by test console

#define RDB_GROUP_DOWNLOAD		0x10
#define RDB_GROUP_UPLOAD		0x20

#define RDB_VAR_SUBCRIBE_TEST	0x40 // subscribed
#define RDB_VAR_SUBCRIBE		0x80 // subscribed

#define RDB_STATIC				0x100

#define RDB_DEVNAME  		"/dev/cdcs_DD"

#define RDB_MAX_LEN          128

typedef struct TRdbNameList
{
    char* szName;
    int bcreate; // create if it does not exit
    int attribute;	//RDB_VAR_XXXX
    char* szValue;	// default value, when it is created
} TRdbNameList;

extern const TRdbNameList g_rdbNameList[];

extern  struct rdb_session *g_rdb_session;

////////////////////////////////////////////////////////////////////////////////
// initilize rdb variable
// session_id (in) -- <=0 no session  id in the rdb variable
// fCreate (in) -- force create all the variable
extern int rdb_init(int session_id, int rdb_group, int fCreate);

////////////////////////////////////////////////////////////////////////////////
// session_id (in) -- <=0 no session  id in the rdb variable
int subscribe(int session_id, int rdb_group, int sub_group);


extern void rdb_end(int session_id,  int rdb_group, int remove_rdb);


////////////////////////
extern int rdb_get_boolean(const char* name, int *value);
////////////////////////////////////////
extern int rdb_set_boolean(const char* name, int value);
//////////////////////////////////////////
extern int rdb_get_uint(const char* name, unsigned int *value);
//////////////////////////////////////////////
extern int rdb_set_uint(const char* name, unsigned int value);

static inline int rdb_set_str(const char* name, const char* value)
{
	//NTCLOG_DEBUG("%s =%s", name, value);
	return rdb_set_string(g_rdb_session, name, value);

}

extern char* rdb_get_i_name(char *buf, const char *namefmt, int i);

extern int rdb_get_i_uint(const char* namefmt, int i, unsigned int *value);
//////////////////////////////////////////////
extern int rdb_set_i_uint(const char* namefmt, int i, unsigned int value);

extern int rdb_get_i_str(const char* namefmt, int i, char* value, int len);

extern int rdb_set_i_str(const char* namefmt, int i, const char* value);

extern int rdb_get_i_boolean(const char* namefmt, int i,  int *value);

extern int rdb_set_i_boolean(const char* namefmt, int i, int value);
// set timestamp rdb, format s.ms
extern int rdb_set_i_timestamp(const char* namefmt, int i, struct timeval *tv);

// poll any rdb changed
// $ >0   --- rdb changed
// $ 0 --- rdb not changed
// $ <0 -- rdb error
extern int poll_rdb(int timeout_sec, int timeout_usec);


#endif
