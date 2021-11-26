/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"
#include "dimclient.h"
#include "serverdata.h"
#include "eventcode.h"
#include "filetransfer.h"
#include "option.h"
#include "timehandler.h"
#include "vouchers.h"

#include "luaEvent.h"

/** Structure of implemented RPC Methods/Functions */
static xsd__string meths[] = {
	"GetRPCMethods",
	"SetParameterValues",
	"GetParameterValues",
	"GetParameterNames",
	"SetParameterAttributes",
	"GetParameterAttributes",
	"AddObject",
	"DeleteObject",
	"Reboot",
#ifdef HAVE_FILE_DOWNLOAD
	"Download",
#endif
#ifdef HAVE_FILE_UPLOAD
	"Upload",
#endif
#ifdef HAVE_FACTORY_RESET
	"FactoryReset",
#endif
#ifdef HAVE_GET_QUEUED_TRANSFERS
	"GetQueuedTransfers",
#endif
#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
	"GetAllQueuedTransfers",
#endif
#ifdef HAVE_SCHEDULE_INFORM
	"ScheduleInform",
#endif
#ifdef HAVE_VOUCHERS_OPTIONS
	"SetVouchers",
	"GetOptions",
#endif
};

/*
 *  CPE methods
 */

int
cwmp__GetRPCMethods (	struct	soap 			*soap,
						void					*_,	/* no input parameter */
						struct	ArrayOfString	*ml)
{
	int ret = SOAP_OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetRPCMethods st: %ld\n", startTime);
	)
		sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);
//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
	ret = OK;
#endif
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ml->__ptrstring = meths; // HH 18.2 Sphairon
		ml->__size = sizeof (meths) / sizeof (xsd__string);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetRPCMethods ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return SOAP_OK;
}

int
cwmp__SetParameterValues (	struct	soap						*soap,
								struct	ArrayOfParameterValueStruct	*ParameterList,
								xsd__string							ParameterKey,
								int									*Status)
{
	int ret;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter SetParameterValues  st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = OK;
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		globalSoap = soap;
		ret = setParameters (ParameterList, Status);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		if (ret != OK) {
			ret = OK;
			*Status = 0;
		}
#endif

		if (ret == OK)
			ret = setParameter (PARAMETER_KEY, ParameterKey);
		if (ret != OK)
			createFault (soap, ret);
	}
	authorizeClient( soap, &info);

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit SetParameterValues ret: %d  lz: %ld\n", ret, getTime () - startTime);
	)
	return ret;
}

int
cwmp__GetParameterValues (	struct	soap						*soap,
								struct	ArrayOfString				*ParameterNames,
								struct ArrayOfParameterValueStruct	*ParameterList)
{
	int ret = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetParameterValues st: %ld\n", startTime);
	)
	sessionCall();
	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
	ret = OK;
#endif

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		globalSoap = soap;
		ret = getParameters (ParameterNames, ParameterList); // HH 18.2 Sphairon

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		if (ret != OK) {
			//ret = getParameters ("InternetGatewayDevice.DeviceInfo.ProductClass", ParameterList);
			struct ArrayOfString temp_namelist;
			static xsd__string dummyStr[] = {"InternetGatewayDevice.DeviceInfo.ProductClass"};

			temp_namelist.__ptrstring = dummyStr;
			temp_namelist.__size = 1;
			ret = getParameters(&temp_namelist, ParameterList);
		}
#endif

		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetParameterValues ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
}

int
cwmp__GetParameterNames (	struct		soap		*soap,
								xsd__string 			ParameterPath,
								xsd__boolean			NextLevel,
								struct	ArrayOfParameterInfoStruct	*ParameterList)
{
	int ret = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetParameterNames st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
	ret = OK;
#endif

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = getParameterNames (ParameterPath, NextLevel, ParameterList);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		if (ret != OK)
			ret = getParameterNames ("InternetGatewayDevice.DeviceInfo.ProductClass", false_, ParameterList);
#endif

		if (ret != OK)
			createFault (soap, ret);

	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetParameterNames ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
}

int
cwmp__SetParameterAttributes		(	struct	soap									*soap,
										struct ArrayOfSetParameterAttributesStruct		*ParameterList,
										struct cwmp__SetParameterAttributesResponse		*empty)
{
	int ret = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter SetParameterAttributes  st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
	ret = OK;
#endif

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = setParametersAttributes (ParameterList);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		ret = OK;
#endif

		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit SetParameterAttributes ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
}

int
cwmp__GetParameterAttributes	(	struct	soap							*soap,
									struct	ArrayOfString					*ParameterNames,
									struct	ArrayOfParameterAttributeStruct	*ParameterList)
{
	int ret = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetParameterAttributes st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
	ret = OK;
#endif

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = getParametersAttributes (ParameterNames, ParameterList);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		if (ret != OK) {
			//ret = getParametersAttributes (ParameterNames, ParameterList);

			struct ArrayOfString temp_namelist;
			static xsd__string dummyStr[] = {"InternetGatewayDevice.DeviceInfo.ProductClass"};

			temp_namelist.__ptrstring = dummyStr;
			temp_namelist.__size = 1;
			ret = getParametersAttributes (&temp_namelist, ParameterList);
		}
#endif

		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetParameterAttributes ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
}

int
cwmp__AddObject	(	struct	soap					*soap,
						xsd__string						ObjectName,
						xsd__string						ParameterKey,
						struct	cwmp__AddObjectResponse	*ReturnValue)
{
	int ret = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter AddObject st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);
	
//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
	ret = OK;
#endif

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = addObject (ObjectName, ReturnValue);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		ret = OK;
#endif

		if (ret == OK)
			ret = setParameter (PARAMETER_KEY, ParameterKey);
		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit AddObject ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
}

int
cwmp__DeleteObject	(	struct	soap	*soap,
							xsd__string		ObjectName,
							xsd__string		ParameterKey,
							xsd__int		*Status)
{
	int ret = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter DeleteObject st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		ret = OK;
#endif

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = deleteObject (ObjectName, Status);

//This is just temporary. This will be taken out, if Motive ACS get fixed.
#if defined(PLATFORM_PLATYPUS) && defined(SKIN_ts)
		ret = OK;
#endif

		if (ret == OK)
			ret = setParameter (PARAMETER_KEY, ParameterKey);
		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit DeleteObject ret: %d lz: %ld\n", ret, getTime () - startTime);
	)
	return ret;
}

int
cwmp__Reboot	(	struct	soap							*soap,
					xsd__string								CommandKey,
					struct			cwmp__RebootResponse	*r)
{
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter Reboot st: %ld\n", startTime);
	)
	sessionCall();
	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	if (CommandKey != NULL)
		setSdLastCommandKey (CommandKey);
	addEventCodeMultiple (EV_M_BOOT, getSdLastCommandKey ());
	setReboot ();
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit Reboot lz: %ld\n", getTime () - startTime);
	)

	return SOAP_OK;
}

int
cwmp__Download		(	struct	soap		*soap,
						xsd__string			CommandKey,
						xsd__string			FileType,
						xsd__string			URL,
						xsd__string			Username,
						xsd__string			Password,
						unsigned	int		FileSize,
						xsd__string			TargetFileName,
						unsigned	int		DelaySeconds,
						xsd__string			SuccessURL,
						xsd__string			FailureURL,
						cwmp__DownloadResponse	*response)
{
#ifdef HAVE_FILE_DOWNLOAD
	int ret;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter Download  st: %ld\n", startTime);
	)
	sessionCall();
	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
		ret = authorizeClient( soap, &info);

	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = execDownload (soap, CommandKey, FileType, URL,
					Username, Password, FileSize,
					TargetFileName, DelaySeconds,
					SuccessURL, FailureURL,
					ACS, NULL, NULL, response);

		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit Download ret: %d  lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

int
cwmp__Upload	(	struct	soap			*soap,
					xsd__string				CommandKey,
					xsd__string				FileType,
					xsd__string				URL,
					xsd__string				Username,
					xsd__string				Password,
					xsd__unsignedInt		DelaySeconds,
					cwmp__UploadResponse	*response)
{
#ifdef HAVE_FILE_UPLOAD
	int ret = OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter Upload st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}

	ret = authorizeClient( soap, &info);
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else	{
		ret = execUpload (soap, CommandKey, FileType, URL,
				  Username, Password, DelaySeconds,
				  ACS, NULL, NULL, response);

		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit Upload ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

/** FactoryReset
 * reset all Parameters to the initial value
 * remove all Eventcodes
 * remove all Options
 * remove all outstanding down-/up-loads
 * and request a reboot
 */
int
cwmp__FactoryReset	(	struct	soap						*soap,
							void								*emptyReq,
							struct	cwmp__FactoryResetResponse	*emptyRes)
{
#ifdef HAVE_FACTORY_RESET
	int ret = OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter FactoryReset st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
	ret = resetAllParameters ();
	if (ret != OK)
		createFault (soap, ret);
	ret = resetEventCodes ();
	if (ret != OK)
		createFault (soap, ret);
#ifdef HAVE_VOUCHERS_OPTIONS
	ret = resetAllOptions ();
	if (ret != OK)
		createFault (soap, ret);
#endif
#ifdef HAVE_FILE
	ret = resetAllFiletransfers();
	if (ret != OK)
		createFault (soap, ret);
#endif /* HAVE_FILE */
	ret = li_event_factoryReset ();
	if (ret != OK)
		createFault (soap, ret);
	setReboot ();
	if (ret != OK)
		createFault (soap, ret);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit FactoryReset ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

int
cwmp__GetQueuedTransfers	(	struct	soap					*soap,
								void							*empty,
								struct	ArrayOfQueuedTransfers	*TransferList)
{
#ifdef HAVE_GET_QUEUED_TRANSFERS
	int	ret = OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetQueuedTransfers st: %ld\n", startTime);
	)

	sessionCall();
	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
	ret = authorizeClient( soap, &info);
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else	{
		ret = execGetQueuedTransfers (TransferList);
		if (ret != OK)
			createFault (soap, ret);
	}

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetQueuedTransfers ret: %d lz: %ld\n", ret, getTime () - startTime);
	)
	return SOAP_OK;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

int
cwmp__GetAllQueuedTransfers	(	struct	soap						*soap,
									void								*empty,
									struct	ArrayOfAllQueuedTransfers	*TransferList)
{
#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
	int	ret = OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetAllQueuedTransfers st: %ld\n", startTime);
	)

	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
	ret = authorizeClient( soap, &info);
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = execGetAllQueuedTransfers (TransferList);
		if (ret != OK)
			createFault (soap, ret);
	}

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetAllQueuedTransfers ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return SOAP_OK;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

int
cwmp__ScheduleInform	(	struct	soap							*soap,
							xsd__unsignedInt						DelaySeconds,
							xsd__string								CommandKey,
							struct	cwmp__ScheduleInformResponse	*emptyRes)
{
#ifdef HAVE_SCHEDULE_INFORM
	int	ret  = 0;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter Schedule st: %ld\n", startTime);
	)

	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
	ret = authorizeClient( soap, &info);
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = execSchedule (soap, DelaySeconds, CommandKey);
		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit Schedule ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return ret;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

int
cwmp__SetVouchers	(	struct	soap						*soap,
						struct	ArrayOfVouchers				*VoucherList,
						struct	cwmp__SetVouchersResponse	*empty)
{
#ifdef HAVE_VOUCHERS_OPTIONS
	int ret = OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter SetVouchers st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
		ret = authorizeClient( soap, &info);
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = storeVouchers (VoucherList);
		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit SetVouchers ret: %d lz: %ld\n", ret, getTime () - startTime);
	)

	return SOAP_OK;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

int
cwmp__GetOptions	(	struct	soap			*soap,
						xsd__string				OptionName,
						struct	ArrayOfOptions	*OptionList)
{
#ifdef HAVE_VOUCHERS_OPTIONS
	int ret = OK;
#ifdef _DEBUG
	long startTime = getTime ();
#endif /* _DEBUG */

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Enter GetOptions st: %ld\n", startTime);
	)
	sessionCall();

	if (soap->header)
	{
		soap->header = analyseHeader (soap->header);
	}
	ret = authorizeClient( soap, &info);
	if ( ret != SOAP_OK )
	{
		createFault (soap, ERR_INTERNAL_ERROR);
	} else
	{
		ret = getOptions (OptionName, OptionList);
		if (ret != OK)
			createFault (soap, ret);
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SOAP, "Exit GetOptions ret: %d lz: %ld\n", ret, getTime () - startTime);
	)
	return ret;
#else
	createFault (soap, ERR_METHOD_NOT_SUPPORTED);
	return ERR_METHOD_NOT_SUPPORTED;
#endif
}

/*
 * ACS methods
 */

/**	Dummy Implementation to satisfy the linker */
int
cwmp__Inform	(	struct	soap						*soap,
					struct	DeviceId					*DeviceId,
					struct	ArrayOfEventStruct			*Event,
					int									MaxEnvelopes,
					time_t								CurrentTime,
					int									RetryCount,
					struct	ArrayOfParameterValueStruct	*ParameterList,
					int									*cMaxEnvelopes)
{
	/* dummy */
	return SOAP_OK;
}

/**	Dummy Implementation to satisfy the linker */
int
cwmp__TransferComplete	(	struct	soap							*soap,
								xsd__string								CommandKey,
								struct	cwmp__Fault						*FaultStruct,
								xsd__dateTime							StartTime,
								xsd__dateTime							CompleteTime,
								struct	cwmp__TransferCompleteResponse	*empty )
{
	/* dummy */
	return SOAP_OK;
}

/**	Dummy Implementation to satisfy the linker */
int cwmp__AutonomousTransferComplete	(	struct	soap										*soap,
												xsd__string											AnnounceURL,
												xsd__string											TransferURL,
												xsd__boolean										IsDownload,
												xsd__string											FileType,
												xsd__unsignedInt									FileSize,
												xsd__string											TargetFileName,
												struct	cwmp__Fault									*FaultStruct,
												xsd__dateTime										StartTime,
												xsd__dateTime										CompleteTime,
												struct	cwmp__AutonomousTransferCompleteResponse	*empty )
{
	/* dummy */
	return SOAP_OK;
}

/**	Dummy Implementation to satisfy the linker */
int
cwmp__RequestDownload		(	struct	soap							*soap,
								xsd__string								FileType,
								struct	ArrayOfArgs						*FileTypeArg,
								struct	cwmp__RequestDownloadResponse	*empty)
{
	/* dummy */
	return SOAP_OK;
}

/**	Dummy Implementation to satisfy the linker */
int
cwmp__Kicked	(	struct	soap	*soap,
					xsd__string		Command,
					xsd__string		Referer,
					xsd__string		Arg,
					xsd__string		Next,
					xsd__string		*NextURL)
{
	/* dummy */
	return SOAP_OK;
}
