/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef FILESTORE_H_
#define FILESTORE_H_

#define 	FILE_MASK	0666

/** FILETRANSFER_STORE_STATUS controls, if the status change in a transferEntry Object
 is written to file or not.
 If defined the status is written, undefined it is not.
 The difference is, after a reboot the download is done again or not, because the status in the
 stored data is not updated.
*/
#define FILETRANSFER_STORE_STATUS

#define FILETRANSFER_MAX_INFO_SIZE 2048

#if defined(PLATFORM_PLATYPUS)

/** Defines the parameter storage directories and files
 */
#define		DEFAULT_PARAMETER_FILE		"/var/tr069/dimclient.conf"
#define		PERSISTENT_PARAMETER_DIR	"/var/tr069/parameter/"
#define		PERSISTENT_DATA_DIR		"/var/tr069/data/"

/** Defines the event storage directories and files
 */
#define		PERSISTENT_FILE			"/var/tr069/bootstrap.dat"
#define		EVENT_STORAGE_FILE		"/var/tr069/eventcode.dat"

/** Defines the option storage directories and files 
 */
#define  	PERSISTENT_OPTION_DIR 		"/var/tr069/options/"
#define  	VOUCHER_FILE			"/var/tr069/vouchers/%d"

#define		PERSISTENT_TRANSFERLIST_DIR 	"/var/tr069/filetransfers/"
#define 	DEFAULT_DOWNLOAD_FILE		"/var/tr069/upload/download.tr069"

#define DEFAULT_DOWNLOAD_FIRMWARE  "/var/tr069/upload/downloadfirmware.tr069"
#define DEFAULT_DOWNLOAD_CONFIGFILE  "/var/tr069/upload/downloadconfig.tr069"

#elif defined(PLATFORM_BOVINE)

/** Defines the parameter storage directories and files
 */
#define		DEFAULT_PARAMETER_FILE		"/etc/dimclient.conf"
#define		PERSISTENT_PARAMETER_DIR	"/usr/local/var/tr-069/parameter/"
#define		PERSISTENT_DATA_DIR		"/usr/local/var/tr-069/data/"

/** Defines the event storage directories and files
 */
#define		PERSISTENT_FILE			"/usr/local/var/tr-069/bootstrap.dat"
#define		EVENT_STORAGE_FILE		"/usr/local/var/tr-069/eventcode.dat"

/** Defines the option storage directories and files 
 */
#define  	PERSISTENT_OPTION_DIR 		"/usr/local/var/tr-069/options/"
#define  	VOUCHER_FILE			"/usr/local/var/tr-069/vouchers/%d"

#define		PERSISTENT_TRANSFERLIST_DIR 	"/usr/local/var/tr-069/filetransfers/"
#define 	DEFAULT_DOWNLOAD_FILE		"/opt/cdcs/upload/download.tr069"

#elif defined(PLATFORM_ARACHNID)

/** Defines the parameter storage directories and files
 */
#define		DEFAULT_PARAMETER_FILE		"/etc/dimclient.conf"
#define		PERSISTENT_PARAMETER_DIR	"/NAND/tr-069/parameter/"
#define		PERSISTENT_DATA_DIR		"/NAND/tr-069/data/"

/** Defines the event storage directories and files
 */
#define		PERSISTENT_FILE			"/NAND/tr-069/bootstrap.dat"
#define		EVENT_STORAGE_FILE		"/NAND/tr-069/eventcode.dat"

/** Defines the option storage directories and files 
 */
#define  	PERSISTENT_OPTION_DIR 		"/NAND/tr-069/options/"
#define  	VOUCHER_FILE			"/NAND/tr-069/vouchers/%d"

#define		PERSISTENT_TRANSFERLIST_DIR 	"/NAND/tr-069/filetransfers/"
#define 	DEFAULT_DOWNLOAD_FILE		"/NAND/tr-069/download.tr069"

#elif defined(PLATFORM_X86)

/** Defines the parameter storage directories and files
 */
#define		DEFAULT_PARAMETER_FILE		"/etc/dimclient.conf"
#define		PERSISTENT_PARAMETER_DIR	"/usr/local/var/tr-069/parameter/"
#define		PERSISTENT_DATA_DIR		"/usr/local/var/tr-069/data/"

/** Defines the event storage directories and files
 */
#define		PERSISTENT_FILE			"/usr/local/var/tr-069/bootstrap.dat"
#define		EVENT_STORAGE_FILE		"/usr/local/var/tr-069/eventcode.dat"

/** Defines the option storage directories and files 
 */
#define  	PERSISTENT_OPTION_DIR 		"/usr/local/var/tr-069/options/"
#define  	VOUCHER_FILE			"/usr/local/var/tr-069/vouchers/%d"

#define		PERSISTENT_TRANSFERLIST_DIR 	"/usr/local/var/tr-069/filetransfers/"
#define 	DEFAULT_DOWNLOAD_FILE		"/usr/local/var/tr-069/download.tr069"

#else

#error "Unknown platform"

#endif

#endif /*FILESTORE_H_*/
