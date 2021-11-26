#ifndef __COMMS_H
#define __COMMS_H
#include <curl/curl.h>
#include "parameters.h"

#define MAX_IFNAME_LEN				32
#define MIN_HTTP_RESPONSE_HEADER	10
#define MAX_PASSWORD		64
#define MAX_HTTP_VERSION	5
#define MAX_URL_LEN			512

#define MAX_SAMPLES		1024

#define MAX_THROUGHPUT	0x7FFFFFFF


typedef struct TConnectSession
{
	TParameters*	m_parameters;

	const char*		m_protocol_name; // protocal name

	char			m_if_name[MAX_IFNAME_LEN+1];	// ifname
	int				m_if_istemp;	// wether the interface is temporary
	int				m_if_istemp_old;	// old m_if_istemp value
	int64_t 		m_if_rx;	// rx bytes from interface
	int64_t 		m_if_tx;	// tx bytes from interface
	int				m_if_rx_pkts;	// rx packets from interface
	int				m_if_tx_pkts;	// tx packets from interface
	char			m_sedge_if_name[MAX_IFNAME_LEN+1];	// smartedge ifname
	int64_t 		m_sedge_if_rx;	// rx bytes from interface
	int64_t 		m_sedge_if_tx;	// tx bytes from interface
	int				m_sedge_if_rx_pkts;	// rx packets from interface
	int				m_sedge_if_tx_pkts;	// tx packets from interface
	
	unsigned int	m_if_cos;	// cos value 0 --tc4, 5 -- tc1, 4 -- tc2

	unsigned int 	m_ServerAddress;//IPv4 address of the UDPEcho server to send pings to. Server
									//must be reachable via the test interface


	unsigned int 	m_ServerPort;//UDP port of the UDPEcho server to send pings to

	char			m_url[MAX_URL_LEN];

	CURL*			m_curl;



	int 			m_connect_timeout_ms; //timeout for connection
	int 			m_session_timeout_ms;// Timeout after 30 seconds stall on recv


	int				m_sample_interval;

	int64_t 		m_last_sample_time;		/// last time ms to take sample
	int64_t		m_last_sample_transfer_bytes;
	int64_t		m_last_sample_raw_transfer_bytes;
	int				m_sample_count;

	int				m_min_throughput;
	int				m_max_throughput;


	struct timeval	m_ROMTime; 		// Request time.

	struct timeval	m_BOMTime; 		//Beginning of download/upload time.
	struct timeval	m_EOMTime; 		//Completion of download/upload time.

	int 			m_stop_by_rdb_change;
	int 			m_test_ready;		// whether download/upload test is ready


}TConnectSession;

// initilize TConnectSession Struct
void init_connection(TConnectSession *pSession,
					TParameters *pParameter,
					const char*pProtocolName);


/// connect to sever address
///$ 0 -- success
///$ <0-- error code
int  make_connection(TConnectSession *pSession);

void close_connection(TConnectSession *pSession);


#endif
