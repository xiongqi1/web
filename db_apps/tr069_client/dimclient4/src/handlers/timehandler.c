/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_FILE

#include <pthread.h>

#include "filetransfer.h"
#include "paramconvenient.h"

#include "luaEvent.h"

#define		NEXT_TIME_REQUEST_DOWNLOAD		2592000 	//1 month

extern pthread_mutex_t informLock;

time_t	StartRequestDownloadTime = 0;

static	time_t	ActRequestDownloadTime = 0;

#ifdef HAVE_SCHEDULE_INFORM

#include "list.h"
#include "eventcode.h"

typedef enum scheduleState { Schedule_NotStarted = 1, Schedule_InProgress = 2, Schedule_Completed = 3 } ScheduleState;

typedef struct ScheduleEntry
{
	// All Data we got from the ACS
	char commandKey[CMD_KEY_STR_LEN+1];
	// StartTime is calculated from now() + delayedSeconds
	time_t startTime;
	// Schedule State
	ScheduleState status;
} ScheduleEntry;

static List scheduleList;

static bool isDelayedSchedule (void);
static int handleDelayedSchedule (struct soap *);
static int handleDelayedScheduleEvents (struct soap *);
static void addSchedule (ScheduleEntry *);

/** Checks the existence of a delayed schedule.
 * if there is an entry in the schedule, then return true
 * else false.
 */
static bool
isDelayedSchedule (void)
{
	return getFirstEntry (&scheduleList) != NULL;
}

/** Iterates through the list of delayed schedules.
 * When find one, it executes the informs the ACS.
 *
 * !!!Attention!!! the list of delayed schedules is not cleared
 * call clearDelayedSchedule () after this function
 */
/**
 \param server	Pointer to the soap data
 \return int	Number of Schedule
 */
static int
handleDelayedSchedule (struct soap *server)
{
	int cnt = 0;
	ListEntry *entry = NULL;
	ScheduleEntry *se = NULL;

/*@*///	printScheduleList (); /*Transfer*/
	while ((entry = iterateList (&scheduleList, entry)))
	{
		se = (ScheduleEntry *) entry->data;
		time_t actTime = time (NULL);
		if (se->status == Schedule_NotStarted && se->startTime <= actTime)
		{
			se->startTime = actTime;
			cnt++;
			se->status = Schedule_InProgress;
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_SCHEDULE, "handleDelayedSchedule\n");
			)
			se->status = Schedule_Completed;
//#ifdef SCHEDULE_STORE_STATUS
/*@*///			writeScheduleEntry (se);
//#endif
			continue;
		}
	}
	return cnt;
}

static int
handleDelayedScheduleEvents( struct soap *server )
{
	int ret = SOAP_OK;
	ListEntry *entry = NULL;
	ScheduleEntry *se = NULL;

	entry = iterateList (&scheduleList, entry);
	while (entry)
	{
		se = (ScheduleEntry *) entry->data;
		if (se->status == Schedule_Completed)
		{
			// It's used in the InformMessage with the SCHEDULED event
			addEventCodeMultiple (EV_M_SCHEDULED, se->commandKey);
		}
		entry = entry->next;
	}
	return ret;
}
/**/

/** For all schedules with status == Schedule_Completed a Inform Message is sent to the ACS
 * and the entry is cleared from the schedule list.
 * In case of an error the status is not changed.
*/
int
clearDelayedSchedule (struct soap *server)
{
	int ret = SOAP_OK;
	ListEntry *entry = NULL;
	ScheduleEntry *se = NULL;

	entry = iterateList (&scheduleList, entry);
	while (entry)
	{
		se = (ScheduleEntry *) entry->data;
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_SCHEDULE, "ClearDelay Schedule Status: %d\n", se->status);
		)
		if (se->status == Schedule_Completed)
		{
/*@*///				deleteScheduleEntry (te);
				efree (se);
				entry = iterateRemove (&scheduleList, entry);
		}
		else
		{
			entry = entry->next;
		}
	}

	return ret;
}

/** Initialize the scheduleList and load the scheduleList from file
 if there is one available
*/
void
initSchedule ( void )
{
	initList (&scheduleList);
/*@*///	return readScheduleList ();
}

static void
addSchedule (ScheduleEntry * se)
{
	addEntry (&scheduleList, se);
/*@*///	writeScheduleEntry (se);
}

int
execSchedule (struct soap *soap,
		xsd__unsignedInt DelaySeconds,
		xsd__string CommandKey)
{
	int returnCode = OK;
	ScheduleEntry *se;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_SCHEDULE, "Delayed Schedule\n");
	)
	se = (ScheduleEntry *) emalloc (sizeof (ScheduleEntry));
	if (se == NULL)
		return ERR_RESOURCE_EXCEED;

	strnCopy (se->commandKey, CommandKey, (sizeof (se->commandKey) - 1));
	se->startTime = time (NULL) + DelaySeconds;
	se->status = Schedule_NotStarted;
	addSchedule (se);

	return returnCode;
}

#endif /* HAVE_SCHEDULE_INFORM */

#ifdef HAVE_REQUEST_DOWNLOAD

#define		FIRMWARE_UPGRADE_IMAGE		"1 Firmware Upgrade Image"
#define		WEB_CONTENT					"2 Web Content"
#define		VENDOR_CONFIGURATION_FILE	"3 Vendor Configuration File"

bool	isRequestDownload = true;

static	bool	RequestDownloadFlag = false;

static int setRequestDownload( void );

static int setRequestDownload( void )
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_REQUEST, "ACS Notification\n");
	)

	addEventCodeSingle (EV_REQUEST_DOWNLOAD);
	RequestDownloadFlag = true;

	return	SOAP_OK;
}

int
clearRequestDownload (struct soap *server)
{
	int ret = SOAP_OK;

	cwmp__ArgStruct	cwmp__Arg_value[1] = {{""/*"NameOne"*/, ""/*"ValueOne"*/}} ;
	cwmp__ArgStruct	*cwmp__Arg_value_ptr = cwmp__Arg_value;
	struct ArrayOfArgs cwmp__FileTypeArg = { &cwmp__Arg_value_ptr, 1 };
	struct cwmp__RequestDownloadResponse empty = {0};

	if( RequestDownloadFlag )
	{
		if (isRequestDownload)	{

			ret = soap_call_cwmp__RequestDownload(server, getServerURL (), "",
													FIRMWARE_UPGRADE_IMAGE,
													&cwmp__FileTypeArg,
													&empty);
		} else {
			ret = SOAP_NO_METHOD;
		}

		RequestDownloadFlag = false;
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_REQUEST, "Call RequestDownload ret: %d\n", ret);
		)
	}
	return ret;
}
#endif /* HAVE_REQUEST_DOWNLOAD */

/** This function creates a timer with a one second delay.
 *  After every Delay the handleDelayedFiletransfers() is called.
 *  If the function returns a value > 0
 *  the "7 TRANSFER COMPLETE" or "10 AUTONOMOUS TRANSFER COMPLETE" event is set,
 * 	communication with the ACS is started.
 *  This function handlers "3 SCHEDULED" and "9 REQUEST DOWNLOAD" events too.
*/
void *
timeHandler (void *localSoap)
{
	int mutexStat;
	struct timespec delay, rem;
	delay.tv_sec = 1;
	delay.tv_nsec = 0;

	while (true)
	{
		nanosleep (&delay, &rem);
		li_event_tick();
		
		// if paramLock is BUSY then the CPE has already a connection to the ACS
		// and we don't want to disturb it.
		if (isDelayedFiletransfer () == true
		    && pthread_mutex_trylock (&informLock) != EBUSY)
		{
			pthread_mutex_unlock (&informLock);
			if (handleDelayedFiletransfers (localSoap) > 0)
			{
				// check if the communication with ACS is already online
				mutexStat =	pthread_mutex_trylock (&informLock);
				if (mutexStat == OK)
				{
					handleDelayedFiletransfersEvents (localSoap);

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_TRANSFER, "Transfer done. Inform ACS\n");
					)

					pthread_mutex_unlock (&informLock);
					setAsyncInform(true);
				}
			}
		}
#ifdef HAVE_SCHEDULE_INFORM
		// if paramLock is BUSY then the CPE has already a connection to the ACS
		// and we don't want to disturb it.
		if (isDelayedSchedule () == true
		    && pthread_mutex_trylock (&informLock) != EBUSY)
		{
			pthread_mutex_unlock (&informLock);
			if (handleDelayedSchedule (localSoap) > 0)
			{
				// check if the communication with ACS is already online
				mutexStat =	pthread_mutex_trylock (&informLock);
				if (mutexStat == OK)
				{
					addEventCodeSingle (EV_SCHEDULED);
					handleDelayedScheduleEvents (localSoap);

					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_SCHEDULE, "Schedule done. Inform ACS\n");
					)

					pthread_mutex_unlock (&informLock);
					setAsyncInform(true);
				}
			}
		}
#endif

#ifdef HAVE_REQUEST_DOWNLOAD
		// if paramLock is BUSY then the CPE has already a connection to the ACS
		// and we don't want to disturb it.
		if (pthread_mutex_trylock (&informLock) != EBUSY)
		{
			pthread_mutex_unlock (&informLock);
			ActRequestDownloadTime = time (NULL);
			if (StartRequestDownloadTime <= ActRequestDownloadTime)
			{
				StartRequestDownloadTime = ActRequestDownloadTime + NEXT_TIME_REQUEST_DOWNLOAD;
				// check if the communication with ACS is already online
				mutexStat =	pthread_mutex_trylock (&informLock);
				if (mutexStat == OK)
				{
					setRequestDownload();
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_REQUEST, "RequestDownload done. Inform ACS\n");
					)
					pthread_mutex_unlock (&informLock);
					setAsyncInform(true);
				}
			}
		}
#endif
		if ( getAsyncInform()
			&& pthread_mutex_trylock (&informLock) != EBUSY) {
				pthread_mutex_unlock (&informLock);
				requestInform();
				setAsyncInform(false);
		}
	}
}
#endif /* HAVE_FILE */
