#ifndef __CONFIG_H__
#define __CONFIG_H__

/* compile options */
#define QMIMGR_CONFIG_SMS_FILE_MODE
//#define QMIMGR_CONFIG_DISABLE_AUTO_TRACKING
/* MC7304 & MC7354 with GobiNet occationally get stuck in read() system call */
#define QMIMGR_CONFIG_READ_FREEZE_WORKAROUND
//#define CONFIG_DISABLE_SCHEDULE_GENERIC_QMI
//#define CONFIG_CDMA_SMS_TEST_FAKEDATA

// qmimgr version and binary name
#define QMIMGR_APP_NAME "qmimgr"
#define QMIMGR_VERSION_MJ	1
#define QMIMGR_VERSION_MN	0

#define WWAN_PREFIX_LENGTH 32

// qmimgr configurations
#define QMIMGR_PORT_WAIT_SEC 30
#define QMIMGR_LASTPORT_WAIT_SEC 10
#define QMIMGR_GENERIC_SCHEDULE_PERIOD 10
#define QMIMGR_SPOOL_SCHEDULE_PERIOD 5
#define QMIMGR_SMS_QUEUE_PERIOD	0
#define QMIMGR_NETCOMM_PROFILE "netcomm"
#define QMIMGR_STARTUP_RETRY_MAX	5

// The values are from Chapter 11 of TS 123.008 standard.
// // T3310 * 5
#define QMIMGR_PS_ATTACH_RESP_TIMEOUT  (15*5)
// // T3321 * 5
#define QMIMGR_PS_DETACH_RESP_TIMEOUT  (15*5)
// // T3380 * 5
#define QMIMGR_PDP_CONT_ACTIVATE_RESP_TIMEOUT  (30*5)
// // T3390 * 5
#define QMIMGR_PDP_CONT_DEACTIVATE_RESP_TIMEOUT (8*5)

// qmimgr memory size configurations
#define QMIMGR_MAX_DB_VARIABLE_LENGTH	256
#define QMIMGR_MAX_DB_VALUE_LENGTH	256
#define QMIMGR_MAX_DB_SMALLVALUE_LENGTH	64
#define QMIMGR_MAX_DB_BIGVALUE_LENGTH	512

#define QMIMGR_AT_COMMAND_BUF	128

#define SMS_MAX_FILE_LENGTH	(64*1024)

#if defined (MODULE_MC7304) || defined (MODULE_MC7430)
#undef NO_LTE_REATTACH

// Technically, in LTE system, LTE Attach Default EPS Bearer is equivalent to the UMTS Attach 
// and then doing a Primary PDP Context establishment procedure.
// This means PS re-attachment should be done to access a network with new APN,
// because LTE attachment include PS attachment and PDP context activation in UMTS.
// However, Sierra MC7304/7430 module automatically performs PS re-attachment procedure after new APN is written.
// (Firmware version 05.05.26.02 for Telstra and 05.05.39.02 for Vodafone)
// (Firmware version 02.14.03.00 for MC7430)
// So the re-attachment routine is taken out for Sierra MC7304/7430 module.
#define NO_LTE_REATTACH
#endif

#endif


