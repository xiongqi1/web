#ifndef __COMMS_H
#define __COMMS_H

#include <netinet/in.h>

#define DEFAULT_SERVER_ADDR	"0.0.0.0"
#define DEFAULT_SERVER_PORT	20010
#define MAX_PACKET_SIZE		128
#define MAX_RDB_BUFFER		64
#define WORKER_THREAD_STACKSIZE 1024*100

typedef struct TParameters
{
	int		g_running;
	int		g_verbosity;
	uint64_t	g_deviceSN;

	int		g_serverPort;
	
} TParameters;

TParameters gParam;

/* Request Message */
/*
Message  	Data Type  	Description
-----------------------------------------------------
MESSAGE_ID  	U16  		0x1051 
*/

/* type of Request message */
typedef struct typeRequest {
#define REQ_ID		0x1051

	uint16_t req_id;
} typeRequest;


/* Response Message */
/*
Reply			Data Type	Description 
-----------------------------------------------------
REPLY_ID		U16		0x9051
RESULT_CODE		U16		Result of the message processing as defined in Appendix B: Common Result Codes
						OK			0x0001	The message was executed correctly.
						NOT_READY_FOR_COMMAND	0x0008	The AG was not in the correct state to receive messages.
						SOFTWARE_ERROR		0x000A	A general software error has occurred in the AG.
DEVICE_SN		U64		Report decimal value of the router's Serial Number
						(Changed from 8 bytes to 64 bits in Rev. B.)
STATUS_DATE		6 bytes		The date on which the message was generated in the format 'DDMMYY'
STATUS_TIME		4 bytes		The time on which the message was generated in the format 'HHMM'
SYSTEM_MODE 		U8		Reports the System Mode in a decimal value. 
						0 = No Service 
						1 = GSM/GPRS mode 
						2 = EDGE 
						3 = UMTS/HSPA mode 
						4 = Wi-Fi mode (Added in Rev. B.)
SIGNAL_QUALITY_RSSI 	U8		Reports the RSSI in a decimal value. 
						0 = -113 dBm or less,  
						1 = -111 dBm,      2 = -109 dBm, 
						3 = -107 dBm,      4 = -105 dBm 
						5 = -103 dBm,      6 = -101 dBm 
						7 = -99 dBm,       8 = -97 dBm 
						9 = -95 dBm,       10 = -93 dBm 
						11 = -91 dBm,      12 = -89 dBm 
						13 = -87 dBm,      14 = -85 dBm 
						15 = -83 dBm,      16 = -81 dBm 
						17 = -79 dBm,      18 = -77 dBm 
						19 = -75 dBm,      20 = -73 dBm 
						21 = -71 dBm,      22 = -69 dBm 
						23 = -67 dBm,      24 = -65 dBm 
						25 = -63 dBm,      26 = -61 dBm 
						27 = -59 dBm,      28 = -57 dBm 
						29 = -55 dBm,      30 = -53 dBm 
						31 = -51 dBm or greater 
						99 = not detectable or SYSTEM_MODE Wi-Fi (Added in Rev. B.)
SIGNAL_QUALITY_BER 	U8		Reports the Bit Error Rate in a decimal value. 
						0 = < 0.2% 
						1 = 0.2% - 0.4%,      2 = 0.4% - 0.8% 
						3 = 0.8% - 1.6%,      4 = 1.6% - 3.2% 
						5 = 3.2% - 6.4%,      6 = 6.4% - 12.8% 
						7 = >12.8% 
						99 = not detectable or SYSTEM_MODE Wi-Fi (Added in Rev. B.)
*/

/* type of response message */
typedef struct typeResponse {
#define REPLY_ID		0x9051
#define RESULT_OK		0x0001
#define RESULT_NOT_READY	0x0008
#define RESULT_ERROR		0x000A

	uint16_t	reply_id;
	uint16_t	result_code;
	uint64_t	device_sn;
	char		status_date[6];
	char		status_time[4];
	uint8_t		system_mode;
	uint8_t		signal_quality_rssi;
	uint8_t		signal_quality_ber;
} __attribute__((packed)) typeResponse;


#endif
