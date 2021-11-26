/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef methods_H
#define methods_H

//gsoap cwmp service name:	dim
//gsoap cwmp service style:	rpc
//gsoap cwmp service encoding:	encoded
//gsoap cwmp service namespace:	urn:dslforum-org:cwmp-1-1
//gsoap cwmp schema  namespace: urn:dslforum-org:cwmp-1-1

/* Define schema types */
typedef char*		 			xsd__string;
typedef int   					xsd__int;
typedef unsigned int 			xsd__unsignedInt;
typedef time_t					xsd__dateTime;
typedef char*					xsd__base64Binary;
typedef struct SOAP_ENC__base64 {
	unsigned char	*__ptr;
	int				__size;
} xsd__base64;

// allows to handle 0|1|true|false as boolean values coming from the ACS
// the client is still sending 0|1
typedef enum _Enum_1 { _0, _1, false_ = 0, true_ = 1}	xsd__boolean;

/** define a special boolean type for the SOAP Header
 * because HoldRequests are only allowed in the message from ACS to CPE
 * but not from CPE to ACS. Therefore we need a custom implementation
 * to ignore this value in the outgoing messages
 */
extern typedef xsd__boolean xsd__boolean_;

//typedef char*				xsd__anyType; /*!!! DON'T MOVE !!!*/

typedef struct Arg
{
	xsd__string Name;
	xsd__string Value;
} cwmp__ArgStruct;

typedef struct SetParameterFaultStruct
{
	xsd__string			ParameterName;
	xsd__unsignedInt 	FaultCode;
	xsd__string			FaultString;
} SetParameterFaultStruct; /* cwmp__SetParameterFaultStruct */

/** EventStruct Definition
*/
typedef struct EventStruct
{
	xsd__string EventCode;
	xsd__string CommandKey;
} cwmp__EventStruct;

typedef struct Option
{
	xsd__string			OptionName;
	xsd__string			VoucherSN;
	xsd__unsignedInt	State;
	xsd__int			Mode;
	xsd__dateTime		StartDate;
	xsd__dateTime		ExpirationDate;
	xsd__boolean		IsTransferable;
} cwmp__Option;

typedef struct AllQueuedTransferStruct
{
	xsd__string			CommandKey;
	xsd__int			State;
	xsd__boolean		IsDownload;
	xsd__string			FileType;
	xsd__unsignedInt	FileSize;
	xsd__string			TargetFileName;
} cwmp__AllQueuedTransferStruct;

typedef struct QueuedTransferStruct
{
	xsd__string			CommandKey;
	xsd__int			State;
} cwmp__QueuedTransferStruct;

typedef struct ParameterAttributeStruct
{
		xsd__string				Name;
		xsd__int 				Notification;
		struct ArrayOfString	*AccessList;
} cwmp__ParameterAttributeStruct;

typedef struct SetParameterAttributesStruct
{
		xsd__string				Name;
		xsd__boolean			NotificationChange;
		xsd__int 				Notification;
		xsd__boolean			AccessListChange;
		struct ArrayOfString	*AccessList;
} cwmp__SetParameterAttributesStruct;

typedef struct ParameterInfoStruct
{
		xsd__string	 Name;
		xsd__boolean Writable;
} cwmp__ParameterInfoStruct;

typedef struct ParameterValueStruct
{
	xsd__string Name;
// If using TypeChecking uncomment the following 2 lines undef ACS_REGMAN
	xsd__int	__typeOfValue;
	void		*Value;
// If not using TypeChecking uncomment the following line define ACS_REGMAN in Makefile
//	xsd__anyType Value;
} cwmp__ParameterValueStruct;

struct ArrayOfString
{
	xsd__string *__ptrstring;
	xsd__int	__size;
};

struct ArrayOfParameterValueStruct
{
	cwmp__ParameterValueStruct **__ptrParameterValueStruct;
	xsd__int __size;
};

struct ArrayOfParameterInfoStruct
{
	cwmp__ParameterInfoStruct **__ptrParameterInfoStruct;
	xsd__int __size;
};

struct ArrayOfSetParameterAttributesStruct
{
	cwmp__SetParameterAttributesStruct **__ptrSetParameterAttributesStruct;
	xsd__int __size;
};

struct cwmp__SetParameterAttributesResponse {
	void *empty;
};

struct ArrayOfParameterAttributeStruct
{
	cwmp__ParameterAttributeStruct **__ptrParameterAttributeStruct;
	xsd__int __size;
};

struct cwmp__AddObjectResponse
{
	xsd__unsignedInt InstanceNumber;
	xsd__int Status;
};

struct cwmp__RebootResponse {
	void *empty;
};

typedef struct DownloadResponse
{
	xsd__int		Status;
	// use xsd__string o/w we can not return an "Unknown Time" 0001-01-01T00:00:00Z
	xsd__string		StartTime;
	xsd__string		CompleteTime;
} cwmp__DownloadResponse;

typedef struct UploadResponse
{
	xsd__int 	Status;
	// use xsd__string o/w we can not return an "Unknown Time" 0001-01-01T00:00:00Z
	xsd__string		StartTime;
	xsd__string		CompleteTime;
} cwmp__UploadResponse;

struct cwmp__FactoryResetResponse {
	void *empty;
};

struct ArrayOfQueuedTransfers
{
	cwmp__QueuedTransferStruct **__ptrQueuedTransferStruct;
	xsd__int			 		__size;
};

struct ArrayOfAllQueuedTransfers
{
	cwmp__AllQueuedTransferStruct **__ptrAllQueuedTransferStruct;
	xsd__int			 __size;
};

struct cwmp__ScheduleInformResponse {
	void *empty;
};

struct ArrayOfVouchers
{
	xsd__base64 *__ptrVoucher;
	xsd__int	__size;
};

struct cwmp__SetVouchersResponse {
	void *empty;
};

struct ArrayOfOptions
{
	cwmp__Option	**__ptrOptionStruct;
	xsd__int		__size;
};

/** DeviceId Definition
	used by Inform()
*/
typedef struct DeviceId
{
	xsd__string Manufacturer;
	xsd__string OUI;
	xsd__string ProductClass;
	xsd__string SerialNumber;
} cwmp__DeviceId;

/** Array of EventStructs
 	used by Inform()
 */
struct ArrayOfEventStruct
{
	cwmp__EventStruct **__ptrEventStruct;
	xsd__int			__size;
};

/** Fault Structure Definitions*/
typedef struct cwmp__Fault
{
	xsd__unsignedInt  			FaultCode;
	xsd__string		  			FaultString;
	int							__sizeParameterValuesFault;
	SetParameterFaultStruct	**SetParameterValuesFault;
} Fault;

struct cwmp__TransferCompleteResponse {
	void *empty;
};

struct cwmp__AutonomousTransferCompleteResponse {
	void *empty;
};

struct ArrayOfArgs
{
	cwmp__ArgStruct 	**__ptrArgStruct;
	xsd__int			__size;
};

struct cwmp__RequestDownloadResponse {
	void *empty;
};

/** Soap Header structure */
struct SOAP_ENV__Header
{
		mustUnderstand xsd__string cwmp__ID;
		mustUnderstand xsd__boolean_ cwmp__HoldRequests;
	//  NoMoreRequests is deprecated. see TR_121
	//  "uncomment" the following line to support tr069 (no ammendments)
	xsd__boolean cwmp__NoMoreRequests;
};

/** Fault detail structure */
typedef struct SOAP_ENV__Detail
{
	Fault					*cwmp__Fault;
} cwmp__Detail;

struct SOAP_ENV__Code
{
	xsd__string 			SOAP_ENV__Value;
    struct SOAP_ENV__Code 	*SOAP_ENV__Subcode;
	xsd__string 			SOAP_ENV__Role;
};

struct OptionStruct
{
        xsd__string		        VSerialNum;
        struct DeviceId			*DeviceId;
        xsd__string             OptionIdent;
        xsd__string             OptionDesc;
        xsd__dateTime           StartDate;
        xsd__int                Duration;
        xsd__string             DurationUnits;
        xsd__string             Mode;
        xsd__boolean            Transferable;
};

typedef struct Object
{
	struct OptionStruct     *Option;
} Object;

struct Signature
{
        int __size;
        Object    *__ptrObject;
};

/*
 *  CPE methods
 */

int
cwmp__GetRPCMethods (
		void					*_,
		struct	ArrayOfString	*MethodList);

int
cwmp__SetParameterValues (
		struct	ArrayOfParameterValueStruct	*ParameterList,
		xsd__string							ParameterKey,
		int									*Status);

int
cwmp__GetParameterValues (
		struct	ArrayOfString				*ParameterNames,
		struct	ArrayOfParameterValueStruct	*ParameterList);

int
cwmp__GetParameterNames (
		xsd__string							ParameterPath,
		xsd__boolean						NextLevel,
		struct	ArrayOfParameterInfoStruct	*ParameterList);

int
cwmp__SetParameterAttributes (
		struct ArrayOfSetParameterAttributesStruct		*ParameterList,
		struct cwmp__SetParameterAttributesResponse		*empty);

int
cwmp__GetParameterAttributes (
		struct	ArrayOfString					*ParameterNames,
		struct	ArrayOfParameterAttributeStruct	*ParameterList);

int
cwmp__AddObject (
		xsd__string						ObjectName,
		xsd__string						ParameterKey,
		struct	cwmp__AddObjectResponse	*ReturnValue);

int
cwmp__DeleteObject (
		xsd__string	ObjectName,
		xsd__string	ParameterKey,
		xsd__int	*Status);

int
cwmp__Reboot (
		xsd__string						CommandKey,
		struct	cwmp__RebootResponse	*r);

int
cwmp__Download (
		xsd__string				CommandKey,
		xsd__string				FileType,
		xsd__string				URL,
		xsd__string				Username,
		xsd__string				Password,
		unsigned	int			FileSize,
		xsd__string				TargetFileName,
		unsigned int			DelaySeconds,
		xsd__string				SuccessURL,
		xsd__string				FailureURL,
		cwmp__DownloadResponse *response);

int
cwmp__Upload (
		xsd__string				CommandKey,
		xsd__string				FileType,
		xsd__string				URL,
		xsd__string				Username,
		xsd__string				Password,
		xsd__unsignedInt		DelaySeconds,
		cwmp__UploadResponse	*response);

int
cwmp__FactoryReset (
		void								*emptyReq,
		struct	cwmp__FactoryResetResponse	*emptyRes);

int
cwmp__GetQueuedTransfers (
		void							*empty,
		struct	ArrayOfQueuedTransfers	*TransferList);

int
cwmp__GetAllQueuedTransfers (
		void								*empty,
		struct	ArrayOfAllQueuedTransfers	*TransferList);

int
cwmp__ScheduleInform (
		xsd__unsignedInt						DelaySeconds,
		xsd__string								CommandKey,
		struct	cwmp__ScheduleInformResponse	*emptyRes);

int
cwmp__SetVouchers (
		struct	ArrayOfVouchers				*VoucherList,
		struct	cwmp__SetVouchersResponse	*empty);

int
cwmp__GetOptions (
		xsd__string				OptionName,
		struct	ArrayOfOptions	*OptionList);

/*
 * ACS methods
 */

/**	Dummy Implementation to satisfy the linker */
int
cwmp__Inform (
		struct								DeviceId	*DeviceId,
		struct								ArrayOfEventStruct	*Event,
		int									MaxEnvelopes,
		time_t								CurrentTime,
		int									RetryCount,
		struct	ArrayOfParameterValueStruct	*ParameterList,
		int									*cMaxEnvelopes);

/**	Dummy Implementation to satisfy the linker */
int
cwmp__TransferComplete (
		xsd__string								CommandKey,
		struct	cwmp__Fault						*FaultStruct,
		xsd__dateTime							StartTime,
		xsd__dateTime							CompleteTime,
		struct	cwmp__TransferCompleteResponse	*empty );

/**	Dummy Implementation to satisfy the linker */
int
cwmp__AutonomousTransferComplete(
		xsd__string											AnnounceURL,
		xsd__string											TransferURL,
		xsd__boolean										IsDownload,
		xsd__string											FileType,
		xsd__unsignedInt									FileSize,
		xsd__string											TargetFileName,
		struct cwmp__Fault									*FaultStruct,
		xsd__dateTime										StartTime,
		xsd__dateTime										CompleteTime,
		struct	cwmp__AutonomousTransferCompleteResponse	*empty );

/**	Dummy Implementation to satisfy the linker */
int
cwmp__RequestDownload (
		xsd__string								FileType,
		struct	ArrayOfArgs						*FileTypeArg,
		struct	cwmp__RequestDownloadResponse	*empty);

/**	Dummy Implementation to satisfy the linker */
int
cwmp__Kicked (
		xsd__string Command,
		xsd__string Referer,
		xsd__string Arg,
		xsd__string Next,
		xsd__string *NextURL);

#endif /* methods_H */
