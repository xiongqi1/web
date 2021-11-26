/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef globals_h
#define globals_h

#include "debug.h"
#include "parameter.h"
#include "paramaccess.h"

/* define EXTENDED_BOOLEAN to handle "true" and "false" from the ACS */ 
#define EXTENDED_BOOLEAN

/* define NO_LOCAL_REBOOT_ON_CHANGE to ignore the reboot flag in the data-model.xml file
 * the reboot must be done by the ACS 
 * therefore a status return code of 1 is returned to signal that the parameter change
 * has be done but not confirmed yet. 
 */ 
#define NO_LOCAL_REBOOT_ON_CHANGE

/*
 * !!!Attention!!! Connection Request is REQUIRED in a CPE
 * but there is an opportunity to switch it off if it is not needed
 * it can save many memory
 */
#define HAVE_CONNECTION_REQUEST

/* define or undefine one or more of the following defines to activate the function 
 */
#define HAVE_OPTIONAL
#ifdef HAVE_OPTIONAL
//# define HAVE_HOST
# define HAVE_RPC_METHODS
//# define HAVE_VOUCHERS_OPTIONS
# define HAVE_FACTORY_RESET
//# define HAVE_KICKED
# define HAVE_FILE
# ifdef HAVE_FILE
#  define HAVE_FILE_DOWNLOAD
#  define HAVE_FILE_UPLOAD

#ifndef PLATFORM_PLATYPUS
#  define HAVE_GET_QUEUED_TRANSFERS
#  define HAVE_GET_ALL_QUEUED_TRANSFERS
#endif

#  ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
#   define HAVE_AUTONOMOUS_TRANSFER_COMPLETE
#  endif /* HAVE_GET_ALL_QUEUED_TRANSFERS */
#  define HAVE_SCHEDULE_INFORM
//#  define HAVE_REQUEST_DOWNLOAD
# endif /* HAVE_FILE */
# define HAVE_DIAGNOSTICS
# ifdef HAVE_DIAGNOSTICS
#  define HAVE_IP_PING_DIAGNOSTICS
//#  define HAVE_WANDSL_DIAGNOSTICS
//#  define HAVE_ATMF5_DIAGNOSTICS
#  ifdef HAVE_FILE
#   define HAVE_UDP_ECHO
#  endif /* HAVE_FILE */
#  ifdef HAVE_FILE_DOWNLOAD
#    define HAVE_DOWNLOAD_DIAGNOSTICS
#  endif /* HAVE_FILE_DOWNLOAD */
#  ifdef HAVE_FILE_UPLOAD
#    define HAVE_UPLOAD_DIAGNOSTICS
#  endif /* HAVE_FILE_UPLOAD */
# endif /* HAVE_DIAGNOSTICS */
#endif /* HAVE_OPTIONAL */

/** Define the following define, if the client should
 * automatically update the NumberOfEntries of an Object
 * The update happens during an AddObject or DeleteObject
 */
#define HANDLE_NUMBERS_OF_ENTRIES
#define NUMBER_OF_ENTRIES_STR		"NumberOfEntries"

#define CONNECTION_REALM			"Dimark"
#define ACS_NOTIFICATION_PARAM		"acscall"

//#define SCHEDULE_STORE_STATUS

#if defined(PLATFORM_PLATYPUS)

/** Host notification port */
#define		HOST_NOTIFICATION_PORT			8081

/** URL ports for host and ACS notification */
#if defined(SKIN_vt)
#define		ACS_NOTIFICATION_PORT			8159
#elif defined(SKIN_ts)
#define		ACS_NOTIFICATION_PORT			7547
#else
#define		ACS_NOTIFICATION_PORT			8082
#endif

/** URL ports for host and ACS notification */
#define		KICKED_NOTIFICATION_PORT		8084
/** The default server port */
#define		UDP_ECHO_DEFAULT_PORT			8088

#define		TRANSFERS_NOTIFICATION_PORT		8096

#else

/** Host notification port */
#define		HOST_NOTIFICATION_PORT			1	/* 8081 */
/** URL ports for host and ACS notification */
#define		ACS_NOTIFICATION_PORT			2	/* 8082 */

/** URL ports for host and ACS notification */
#define		KICKED_NOTIFICATION_PORT		4	/* 8084 */
/** The default server port */
#define		UDP_ECHO_DEFAULT_PORT			8	/* 8088 */

#define		TRANSFERS_NOTIFICATION_PORT		16	/* 8096 */

#endif

#define		ACS_UDP_NOTIFICATION_PORT		1600

#define		MAX_PATH_NAME_SIZE		400
#define 	MAX_PARAM_PATH_LENGTH 	257
#define		MAX_EVENT_LIST_NUM		64
#define		CMD_KEY_STR_LEN			32
#define		MAX_CONF_FILE_SRT_LEN   512


/* Definition of "Unknown Time" from TR121 */
#define UNKNOWN_TIME				"0001-01-01T00:00:00Z"
#define DATE_TIME_LENGHT			21

/* Definition of all ErrorCodes */
#define NO_ERROR						0
#define OK								0
#define DIAG_ERROR						-1

#define ERR_METHOD_NOT_SUPPORTED 		9000
#define ERR_REQUEST_DENIED			9001
#define ERR_INTERNAL_ERROR			9002
#define ERR_INVALID_ARGUMENT			9003
#define ERR_RESOURCE_EXCEED			9004
#define ERR_INVALID_PARAMETER_NAME		9005
#define ERR_INVALID_PARAMETER_TYPE		9006
#define ERR_INVALID_PARAMETER_VALUE		9007
#define ERR_READONLY_PARAMETER    		9008
#define ERR_WRITEONLY_PARAMETER   		9008
#define ERR_NOTIFICATION_REQ_REJECT		9009
#define ERR_DOWNLOAD_FAILURE			9010
#define ERR_UPLOAD_FAILURE			9011
#define ERR_TRANS_AUTH_FAILURE			9012
#define ERR_NO_TRANS_PROTOCOL			9013

#define ERR_JOIN_MULTICAST_GROUP		9014	/* Download failure: unable to join multicast group */
#define ERR_CONTACT_FILE_SERVER			9015	/* Download failure: unable to contact file server */
#define ERR_ACCESS_FILE				9016	/* Download failure: unable to access file */
#define ERR_COMPLETE_DOWNLOAD			9017	/* Download failure: unable to complete download */
#define ERR_CORRUPTED				9018	/* Download failure: file corrupted */
#define ERR_AUTHENTICATION			9019	/* Download failure: file authentication failure */

#define 	ERR_NO_INFORM_DONE			9890
// Error during opening for writing the event storage file
#define 	ERR_DIM_EVENT_WRITE			9800
// Error during opening for reading or reading the event storage
#define 	ERR_DIM_EVENT_READ			9801
// Error during creating or deleting one of markers for 
// detecting boot or bootstrap
#define 	ERR_DIM_MARKER_OP			9805
// Error during reading or writing the file transfer informations
// from or into the storage 
#define 	ERR_DIM_TRANSFERLIST_WRITE	9810
#define 	ERR_DIM_TRANSFERLIST_READ	9811
// Error during handling options
#define 	ERR_INVALID_OPTION_NAME		9820
#define		ERR_CANT_DELETE_OPTION		9821
#define 	ERR_READ_OPTION				9822
#define 	ERR_WRITE_OPTION			9823
// Error reading the initial parameter file
// or data from storage 
// or parameter metadata from storage 
#define 	ERR_READ_PARAMFILE			9830
// Error during writing data into storage
// or parameter metadata into storage
#define 	ERR_WRITE_PARAMFILE			9831

/* Definition of all EventCodes */

/* Cumulative Behavior - Single */
#define EV_BOOTSTRAP				"0 BOOTSTRAP"
#define EV_BOOT						"1 BOOT"
#define EV_PERIODIC					"2 PERIODIC"
#define EV_SCHEDULED				"3 SCHEDULED"
#define EV_VALUE_CHANGE				"4 VALUE CHANGE"
#ifdef HAVE_KICKED
#define EV_KICKED					"5 KICKED"
#endif
#define EV_CONNECT_REQ				"6 CONNECTION REQUEST"
#define EV_TRANSFER_COMPLETE		"7 TRANSFER COMPLETE"
#define EV_DIAG_COMPLETE			"8 DIAGNOSTICS COMPLETE"
#ifdef HAVE_REQUEST_DOWNLOAD
#define EV_REQUEST_DOWNLOAD			"9 REQUEST DOWNLOAD"
#endif
#define EV_AUTONOMOUS_TRANSFER_COMPLETE		"10 AUTONOMOUS TRANSFER COMPLETE"

/* Cumulative Behavior - Multiple */
#define EV_M_BOOT					"M Reboot"
#define EV_M_SCHEDULED				"M ScheduleInform"
#define EV_M_DOWNLOAD				"M Download"
#define EV_M_UPLOAD					"M Upload"

/* "M "<method name>
X <OUI> <event> */

// Defines for DoS attack recognition
// length of the time slot in seconds
#define MAX_REQUEST_TIME	100
// the number of allowed requests in one time slot
#define MAX_REQUESTS_PER_TIME 10
/* Enable notification */
#define	HAVE_NOTIFICATION

typedef int (*accessParam) (const char *, ParameterType, ParameterValue *);

/**
 * ManagementServer parameters from a config
 */
struct ConfigManagement
{
	char url[1024];		    /* ManagementServerURL */
	char username[128];		/*  ManagementServerUsername*/
	char password[128];		/*  ManagementServerPassword ) */
	unsigned int notificationTime;	/* Time for notification */
};

typedef struct func
{
	int idx;
	accessParam func;
} Func;
// Who initiated transfer
enum Transfers_type { ACS, NOTACS };

#endif /* globals_h */
