/*
 * fault_mgmt.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system provides an solution to report system faults through TR-069.
 *
 * This file includes the implementation of functions defined in fault_mgmt.h
 * and other functions used handle FaultMgmt objects of RDB.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "rdb_ops.h"

#include "fm_errno.h"
#include "fm_shared.h"
#include "fm_event.h"
#include "fm_current_alarm.h"
#include "fm_supported_alarm.h"
#include "fault_mgmt.h"

#define DEFAULT_HISTORY_EVENT_NUMBER        100
#define DEFAULT_EXPEDITED_EVENT_NUMBER      15
#define DEFAULT_QUEUED_EVENT_NUMBER         15
#define DEFAULT_SPARE_EVENT_NUMBER          10
#define DEFAULT_MAX_EVENT_ENTRY             DEFAULT_HISTORY_EVENT_NUMBER + DEFAULT_EXPEDITED_EVENT_NUMBER + DEFAULT_QUEUED_EVENT_NUMBER + DEFAULT_SPARE_EVENT_NUMBER


/****************************************************
 *  RDB Definitions
 ****************************************************/
enum FaultMgmtParameter {
    SupportedAlarmNumberOfEntries = 0,
    MaxCurrentAlarmEntries,
    CurrentAlarmNumberOfEntries,
    HistoryEventNumberOfEntries,
    ExpeditedEventNumberOfEntries,
    QueuedEventNumberOfEntries,
    _MaxEventEntries,
    _IndexEvent,
    _IndexHistoryEvent,
    _IndexExpeditedEvent,
    _IndexQueuedEvent,
    FaultMgmtParameterCount,
};

const char *FAULT_MGMT_PARA_NAME[] = {
    [SupportedAlarmNumberOfEntries]     = "SupportedAlarmNumberOfEntries",
    [MaxCurrentAlarmEntries]            = "MaxCurrentAlarmEntries",
    [CurrentAlarmNumberOfEntries]       = "CurrentAlarmNumberOfEntries",
    [HistoryEventNumberOfEntries]       = "HistoryEventNumberOfEntries",
    [ExpeditedEventNumberOfEntries]     = "ExpeditedEventNumberOfEntries",
    [QueuedEventNumberOfEntries]        = "QueuedEventNumberOfEntries",
    [_MaxEventEntries]                  = "_MaxEventEntries",
    [_IndexEvent]                       = "_IndexEvent",
    [_IndexHistoryEvent]                = "_IndexHistoryEvent",
    [_IndexExpeditedEvent]              = "_IndexExpeditedEvent",
    [_IndexQueuedEvent]                 = "_IndexQueuedEvent",
};

const int FAULT_MGMT_PARA_DEFAULT_VAL[] = {
    [SupportedAlarmNumberOfEntries]     = 0,
    [MaxCurrentAlarmEntries]            = 0,
    [CurrentAlarmNumberOfEntries]       = 0,
    [HistoryEventNumberOfEntries]       = DEFAULT_HISTORY_EVENT_NUMBER,
    [ExpeditedEventNumberOfEntries]     = DEFAULT_EXPEDITED_EVENT_NUMBER,
    [QueuedEventNumberOfEntries]        = DEFAULT_QUEUED_EVENT_NUMBER,

    /* The actual _MaxEventEntries will add SupportedAlarmNumberOfEntries after
     * loading the supported alarm list */
    [_MaxEventEntries]                  = DEFAULT_MAX_EVENT_ENTRY,
    [_IndexEvent]                       = DEFAULT_MAX_EVENT_ENTRY - 1,
    [_IndexHistoryEvent]                = DEFAULT_HISTORY_EVENT_NUMBER - 1,
    [_IndexExpeditedEvent]              = DEFAULT_EXPEDITED_EVENT_NUMBER - 1,
    [_IndexQueuedEvent]                 = DEFAULT_QUEUED_EVENT_NUMBER - 1,
};


/****************************************************
 *  Implementation Object Definitions
 ****************************************************/
typedef struct _FaultMgmt {
    int SupportedAlarmNumberOfEntries;
    int MaxCurrentAlarmEntries;
    int CurrentAlarmNumberOfEntries;
    int HistoryEventNumberOfEntries;
    int ExpeditedEventNumberOfEntries;
    int QueuedEventNumberOfEntries;
    int _MaxEventEntries;
    int _IndexEvent;
    int _IndexHistoryEvent;
    int _IndexExpeditedEvent;
    int _IndexQueuedEvent;
} FaultMgmt;

/*****************************************/

enum FAULT_LOG_ACTION_TYPE {
    ACTION_CLEAR = 0,
    ACTION_SET,
};

struct rdb_session *session = NULL;
SupportedAlarmInfo **alarm_info = NULL;
static SupportedAlarm **alarm = NULL;
int alarm_count = -1;

static int read_fault_mgmt(FaultMgmt *fault_mgmt);
static int save_fault_mgmt(const FaultMgmt *fault_mgmt);

/****************************************************
 *  Functions for fmctl boot only
 ****************************************************/
/**
 * @brief      Creates RDB default objects for FaultMgmt.
 *
 * @return     SUCCESS or FAILED_LOCK_RDB
 */
int create_db_default_objects(void)
{
    char tmp_val[1] = {'\0'};

    assert(session != NULL);

    if (get_rdb_lock() == 0)
    {
        if (rdb_getinfo(session, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM FAULT_MGMT_DB_INDEX_SUFFIX, NULL, NULL, NULL) == -ENOENT)
        {
            add_or_update(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM FAULT_MGMT_DB_INDEX_SUFFIX, tmp_val, sizeof(tmp_val), PERSIST_FLAG_SUPPORTED_ALARM);
        }
        if (rdb_getinfo(session, FAULT_MGMT_DB_PATH_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, NULL, NULL, NULL) == -ENOENT)
        {
            add_or_update(FAULT_MGMT_DB_PATH_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, tmp_val, sizeof(tmp_val), PERSIST_FLAG_EVENT);
        }
        if (rdb_getinfo(session, FAULT_MGMT_DB_PATH_CURRENT_ALARM FAULT_MGMT_DB_INDEX_SUFFIX, NULL, NULL, NULL) == -ENOENT)
        {
            add_or_update(FAULT_MGMT_DB_PATH_CURRENT_ALARM FAULT_MGMT_DB_INDEX_SUFFIX, tmp_val, sizeof(tmp_val), PERSIST_FLAG_EVENT);
        }
        if (rdb_getinfo(session, FAULT_MGMT_DB_PATH_HISTORY_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, NULL, NULL, NULL) == -ENOENT)
        {
            add_or_update(FAULT_MGMT_DB_PATH_HISTORY_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, tmp_val, sizeof(tmp_val), PERSIST_FLAG_EVENT);
        }
        if (rdb_getinfo(session, FAULT_MGMT_DB_PATH_EXPEDITED_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, NULL, NULL, NULL) == -ENOENT)
        {
            add_or_update(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, tmp_val, sizeof(tmp_val), PERSIST_FLAG_EVENT);
        }
        if (rdb_getinfo(session, FAULT_MGMT_DB_PATH_QUEUED_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, NULL, NULL, NULL) == -ENOENT)
        {
            add_or_update(FAULT_MGMT_DB_PATH_QUEUED_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, tmp_val, sizeof(tmp_val), PERSIST_FLAG_EVENT);
        }

        put_rdb_lock();
        return SUCCESS;
    }
    else
        return FAILED_LOCK_RDB;
}

/**
 * @brief      Creates RDB default settings for FaultMgmt. This function needs to
 *             be called after SupportedAlarms have been loadded.
 *
 * @return     SUCCESS or FAILED_LOCK_RDB
 */
int create_db_default_settings(void)
{
    FaultMgmt fault_mgmt;
    int new_max_event_entry = 0, changed = 0;

    assert(session != NULL);

    if (get_rdb_lock() == 0)
    {
        if (read_fault_mgmt(&fault_mgmt) != SUCCESS)
        {
            fault_mgmt.CurrentAlarmNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[CurrentAlarmNumberOfEntries];
            fault_mgmt.HistoryEventNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[HistoryEventNumberOfEntries];
            fault_mgmt.ExpeditedEventNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[ExpeditedEventNumberOfEntries];
            fault_mgmt.QueuedEventNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[QueuedEventNumberOfEntries];
            fault_mgmt._IndexHistoryEvent = FAULT_MGMT_PARA_DEFAULT_VAL[_IndexHistoryEvent];
            fault_mgmt._IndexExpeditedEvent = FAULT_MGMT_PARA_DEFAULT_VAL[_IndexExpeditedEvent];
            fault_mgmt._IndexQueuedEvent = FAULT_MGMT_PARA_DEFAULT_VAL[_IndexQueuedEvent];

            fault_mgmt.SupportedAlarmNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[SupportedAlarmNumberOfEntries] + alarm_count;
            fault_mgmt.MaxCurrentAlarmEntries = FAULT_MGMT_PARA_DEFAULT_VAL[MaxCurrentAlarmEntries] + alarm_count;
            fault_mgmt._MaxEventEntries = FAULT_MGMT_PARA_DEFAULT_VAL[_MaxEventEntries] + alarm_count;
            fault_mgmt._IndexEvent = FAULT_MGMT_PARA_DEFAULT_VAL[_IndexEvent] + alarm_count;
            changed++;
        }
        else
        {
            /* When new supported alarms are added, MaxCurrentAlarmEntries and _MaxEventEntries should both increase */
            if (fault_mgmt.SupportedAlarmNumberOfEntries < FAULT_MGMT_PARA_DEFAULT_VAL[SupportedAlarmNumberOfEntries] + alarm_count)
            {
                fault_mgmt.SupportedAlarmNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[SupportedAlarmNumberOfEntries] + alarm_count;
                fault_mgmt.MaxCurrentAlarmEntries = FAULT_MGMT_PARA_DEFAULT_VAL[MaxCurrentAlarmEntries] + alarm_count;
                changed++;
            }

            if (fault_mgmt.HistoryEventNumberOfEntries < FAULT_MGMT_PARA_DEFAULT_VAL[HistoryEventNumberOfEntries])
            {
                fault_mgmt.HistoryEventNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[HistoryEventNumberOfEntries];
                if (get_event_referrer(fault_mgmt._IndexHistoryEvent, HISTORY_EVENT_NAME, NULL) != SUCCESS)
                    fault_mgmt._IndexHistoryEvent = FAULT_MGMT_PARA_DEFAULT_VAL[HistoryEventNumberOfEntries] - 1;
                changed++;
            }

            if (fault_mgmt.ExpeditedEventNumberOfEntries < FAULT_MGMT_PARA_DEFAULT_VAL[ExpeditedEventNumberOfEntries])
            {
                fault_mgmt.ExpeditedEventNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[ExpeditedEventNumberOfEntries];
                if (get_event_referrer(fault_mgmt._IndexExpeditedEvent, EXPEDITED_EVENT_NAME, NULL) != SUCCESS)
                    fault_mgmt._IndexExpeditedEvent = FAULT_MGMT_PARA_DEFAULT_VAL[ExpeditedEventNumberOfEntries] - 1;
                changed++;
            }

            if (fault_mgmt.QueuedEventNumberOfEntries < FAULT_MGMT_PARA_DEFAULT_VAL[QueuedEventNumberOfEntries])
            {
                fault_mgmt.QueuedEventNumberOfEntries = FAULT_MGMT_PARA_DEFAULT_VAL[QueuedEventNumberOfEntries];
                if (get_event_referrer(fault_mgmt._IndexQueuedEvent, QUEUED_EVENT_NAME, NULL) != SUCCESS)
                    fault_mgmt._IndexQueuedEvent = FAULT_MGMT_PARA_DEFAULT_VAL[QueuedEventNumberOfEntries] - 1;
                changed++;
            }

            /* Possible to support other event size expansion */
            new_max_event_entry = fault_mgmt.SupportedAlarmNumberOfEntries +
                                  fault_mgmt.HistoryEventNumberOfEntries +
                                  fault_mgmt.ExpeditedEventNumberOfEntries +
                                  fault_mgmt.QueuedEventNumberOfEntries +
                                  DEFAULT_SPARE_EVENT_NUMBER;
            if (fault_mgmt._MaxEventEntries < new_max_event_entry)
            {
                fault_mgmt._MaxEventEntries = new_max_event_entry;
                /* Change event index if no event exists so next event starts from 0 */
                if (get_event(fault_mgmt._IndexEvent, NULL) != SUCCESS)
                    fault_mgmt._IndexEvent = new_max_event_entry - 1;
                changed++;
            }

        }

        if (changed > 0)
            save_fault_mgmt(&fault_mgmt);
        put_rdb_lock();
        return SUCCESS;
    }
    else
        return FAILED_LOCK_RDB;
}

/****************************************************
 *  Local Support Functions
 ****************************************************/
static SupportedAlarm *get_alarm_by_id(int alarm_type_id)
{
    int i;

    if (!alarm)
        return NULL;

    for (i = 0; i < alarm_count; i++)
    {
        if (alarm[i]->_event_type_id == alarm_type_id)
            return alarm[i];
    }

    /* If reaches here, the alarm type is not found. We will reload the list in case it is updated */
    free_supported_alarms(&alarm, &alarm_info, &alarm_count);
    if (get_supported_alarms(&alarm, &alarm_info, &alarm_count) != SUCCESS)
        return NULL;

    for (i = 0; i < alarm_count; i++)
    {
        if (alarm[i]->_event_type_id == alarm_type_id)
            return alarm[i];
    }

    return NULL;
}

static int save_fault_mgmt_para(int para[FaultMgmtParameterCount])
{
    char szname[256], tmp_val[16];
    int i;

    assert(session != NULL && para != NULL);

    for (i = 0; i < FaultMgmtParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH ".%s", FAULT_MGMT_PARA_NAME[i]);

        // When we save it to RDB, we add 1 offset to all indexes so it aligns with
        // the saved event objects
        if (i == _IndexEvent || i == _IndexHistoryEvent || i == _IndexExpeditedEvent || i == _IndexQueuedEvent)
        {
            sprintf(tmp_val, "%d", para[i] + 1);
        }
        else
        {
            sprintf(tmp_val, "%d", para[i]);
        }
        if (add_or_update(szname, tmp_val, strlen(tmp_val) + 1, PERSIST_FLAG_EVENT) != 0)
        {
            continue;
        }
    }

    return SUCCESS;
}

static int read_fault_mgmt_para(int para[FaultMgmtParameterCount])
{
    char szname[256], tmp_val[16];
    int len = sizeof(tmp_val), i;

    assert(session != NULL && para != NULL);

    for (i = 0; i < FaultMgmtParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH ".%s", FAULT_MGMT_PARA_NAME[i]);
        len = sizeof(tmp_val);
        if (rdb_get(session, szname, tmp_val, &len) != 0)
            return FAILED_GET_PARA;

        // When we read from RDB, we reduce 1 offset to use 0-starting indexing system
        if (i == _IndexEvent || i == _IndexHistoryEvent || i == _IndexExpeditedEvent || i == _IndexQueuedEvent)
        {
            para[i] = atoi(tmp_val) - 1;
        }
        else
        {
            para[i] = atoi(tmp_val);
        }
    }
    return SUCCESS;
}

/**
 * @brief      Save FaultMgmt object values into RDB. Nothing of fault_mgmt object is
 *             changed.
 *
 * @param      fault_mgmt  Pointer to the fault management object
 *
 * @return     SUCESS or FAILED_SET_PARA
 */
static int save_fault_mgmt(const FaultMgmt *fault_mgmt)
{
    int parameters[FaultMgmtParameterCount];

    assert(session != NULL && fault_mgmt != NULL);

    parameters[SupportedAlarmNumberOfEntries] = fault_mgmt->SupportedAlarmNumberOfEntries;
    parameters[MaxCurrentAlarmEntries] = fault_mgmt->MaxCurrentAlarmEntries;
    parameters[CurrentAlarmNumberOfEntries] = fault_mgmt->CurrentAlarmNumberOfEntries;
    parameters[HistoryEventNumberOfEntries] = fault_mgmt->HistoryEventNumberOfEntries;
    parameters[ExpeditedEventNumberOfEntries] = fault_mgmt->ExpeditedEventNumberOfEntries;
    parameters[QueuedEventNumberOfEntries] = fault_mgmt->QueuedEventNumberOfEntries;
    parameters[_MaxEventEntries] = fault_mgmt->_MaxEventEntries;
    parameters[_IndexEvent] = fault_mgmt->_IndexEvent;
    parameters[_IndexHistoryEvent] = fault_mgmt->_IndexHistoryEvent;
    parameters[_IndexExpeditedEvent] = fault_mgmt->_IndexExpeditedEvent;
    parameters[_IndexQueuedEvent] = fault_mgmt->_IndexQueuedEvent;

    return save_fault_mgmt_para(parameters);
}

/**
 * @brief      Reads FaultMgmt values into an object. There is no space allocated
 *             so the fault_mgmt object does not need to be released/put later.
 *
 * @param[OUT] fault_mgmt  Pointer to the fault management object
 *
 * @return     SUCESS or FAILED_GET_PARA
 */
static int read_fault_mgmt(FaultMgmt *fault_mgmt)
{
    int ret;
    int parameters[FaultMgmtParameterCount];

    assert(session != NULL && fault_mgmt != NULL);

    if ((ret = read_fault_mgmt_para(parameters)) != SUCCESS)
        return ret;

    fault_mgmt->SupportedAlarmNumberOfEntries = parameters[SupportedAlarmNumberOfEntries];
    fault_mgmt->MaxCurrentAlarmEntries = parameters[MaxCurrentAlarmEntries];
    fault_mgmt->CurrentAlarmNumberOfEntries = parameters[CurrentAlarmNumberOfEntries];
    fault_mgmt->HistoryEventNumberOfEntries = parameters[HistoryEventNumberOfEntries];
    fault_mgmt->ExpeditedEventNumberOfEntries = parameters[ExpeditedEventNumberOfEntries];
    fault_mgmt->QueuedEventNumberOfEntries = parameters[QueuedEventNumberOfEntries];
    fault_mgmt->_MaxEventEntries = parameters[_MaxEventEntries];
    fault_mgmt->_IndexEvent = parameters[_IndexEvent];
    fault_mgmt->_IndexHistoryEvent = parameters[_IndexHistoryEvent];
    fault_mgmt->_IndexExpeditedEvent = parameters[_IndexExpeditedEvent];
    fault_mgmt->_IndexQueuedEvent = parameters[_IndexQueuedEvent];
    return SUCCESS;
}

/**
 * @brief      Read the status of a given alarm type.
 *
 * @param[in]  alarm_type_id  The alarm type identifier
 *
 * @return     When succeeded:
 *                  0 - Alarm is not set
 *                  1 - Alarm is set
 *             Upon failures:
 *                  FAILED_FM_UNINITIALIZED
 *                  FAILED_INVALID_ALARM_ID
 */
static int check_alarm_status(int alarm_type_id)
{
    int ret, i;
    FaultMgmt fault_mgmt;

    if (!session)
        return FAILED_FM_UNINITIALIZED;

    if (get_alarm_by_id(alarm_type_id) == NULL)
        return FAILED_INVALID_ALARM_ID;

    if ((ret = read_fault_mgmt(&fault_mgmt)) != SUCCESS)
        return ret;

    ret = 0;
    if (fault_mgmt.CurrentAlarmNumberOfEntries > 0)
    {
        for (i = 0; i < fault_mgmt.CurrentAlarmNumberOfEntries; i++)
        {
            if (get_current_alarm_event_type_id(i) == alarm_type_id)
            {
                ret = 1;
                break;
            }
        }
    }

    return ret;
}

/**
 * @brief      Log/clear FaultMgmt alarms. The design document is located at
 *             https://pdgwiki.netcommwireless.com/mediawiki/index.php/TR-069_FaultMgmt_Design#Adding_Supported_Alarms
 *
 * @param      alarm_type_id    The SupportedAlarm type ID
 * @param      action           The action (ACTION_CLEAR or ACTION_SET)
 * @param      additional_text  The additional text
 * @param      additional_info  The additional information
 *
 * @return     { description_of_the_return_value }
 */
static int do_fault_mgmt_log(int alarm_type_id, int action, const char *additional_text, const char *additional_info)
{
    /* No space is allocated for supported_alarm_item or new_event */
    SupportedAlarm *supported_alarm_item;
    Event new_event;

    FaultMgmt fault_mgmt;
    EventReferrer event_referrer;
    CurrentAlarm current_alarm, current_alarm_last;

    int i, current_alarm_ready, current_alarm_status = 0;
    char tmp_alarm_identifier_str[32], tmp_time_str[32];
    time_t time_stamp = time(NULL);

    if ((supported_alarm_item = get_alarm_by_id(alarm_type_id)) == NULL)
    {
        return FAILED_INVALID_ALARM_ID;
    }

    current_alarm_status = check_alarm_status(alarm_type_id);
    if (action == ACTION_CLEAR && current_alarm_status == 0)
    {
        return SUCCESS;
    }

    /* If reporting mechanism is not set or disabled, we should skip record.
     * Even when we need to skip recording, we should not ignore the clearing the message if
     * it has been set */
    if ((!supported_alarm_item->ReportingMechanism ||
         (strcmp(supported_alarm_item->ReportingMechanism, REPORT_MECHANISM_OPT_EXPEDITED) != 0 &&
          strcmp(supported_alarm_item->ReportingMechanism, REPORT_MECHANISM_OPT_QUEUED) != 0 &&
          strcmp(supported_alarm_item->ReportingMechanism, REPORT_MECHANISM_OPT_LOGGED) != 0))
         && !(action == ACTION_CLEAR && current_alarm_status != 0))
    {
        return SUCCESS;
    }

    /* Load values from database */
    if (read_fault_mgmt(&fault_mgmt) != 0)
    {
        return FAILED_GET_PARA;
    }

    /* Find an available Event slot */
    do
    {
        fault_mgmt._IndexEvent++;
        if (fault_mgmt._IndexEvent >= fault_mgmt._MaxEventEntries)
            fault_mgmt._IndexEvent = 0;

        /* i = FAILED_PARA_NOT_EXIST or 0 when the slot is available */
        if ((i = get_event_ref_count(fault_mgmt._IndexEvent)) == FAILED_GET_PARA)
        {
            return FAILED_GET_PARA;
        }
    } while (i > 0);

    /* Create an event to save on the slot */
    // Time is using timestamp
    time(&time_stamp);
    sprintf(tmp_time_str, "%ld", time_stamp);
    new_event.EventTime = tmp_time_str;

    // Identifier is a combination of alarm_type_id and event index
    sprintf(tmp_alarm_identifier_str, "TID%.5d_IDX%.5d", alarm_type_id, fault_mgmt._IndexEvent + 1);
    new_event.AlarmIdentifier = tmp_alarm_identifier_str;

    // new_event.NotificationType will be set later if action != ACTION_CLEAR
    if (action == ACTION_CLEAR)
        new_event.NotificationType = EVENT_NTFY_TYPE_OPT_CLEARED;

    // Any unsupported severity value will be treated as INDETERMINATE
    if (strcmp(supported_alarm_item->PerceivedSeverity, SEVERITY_OPT_CRITICAL) == 0 ||
        strcmp(supported_alarm_item->PerceivedSeverity, SEVERITY_OPT_MAJOR) == 0 ||
        strcmp(supported_alarm_item->PerceivedSeverity, SEVERITY_OPT_MINOR) == 0 ||
        strcmp(supported_alarm_item->PerceivedSeverity, SEVERITY_OPT_WARNING) == 0 ||
        strcmp(supported_alarm_item->PerceivedSeverity, SEVERITY_OPT_INDETERMINATE) == 0)
        new_event.PerceivedSeverity = supported_alarm_item->PerceivedSeverity;
    else
        new_event.PerceivedSeverity = EVENT_SEVERITY_OPT_INDETERMINATE;

    new_event.ManagedObjectInstance = "";
    new_event.AdditionalText = (char *)(additional_text == NULL ? "" : additional_text);
    new_event.AdditionalInformation = (char *)(additional_info == NULL ? "" : additional_info);
    new_event._ref_count = 0;
    new_event._event_type_id = alarm_type_id;

    /* Update current alarm */
    for (i = 0; i <= fault_mgmt.MaxCurrentAlarmEntries; i++)
    {
        if (i < fault_mgmt.CurrentAlarmNumberOfEntries)
        {
            if (get_current_alarm_event_type_id(i) == alarm_type_id)
            {
                current_alarm_ready = (get_current_alarm(i, &current_alarm) == SUCCESS);

                /* In case current alarm was not loaded properly */
                if (!current_alarm_ready)
                {
                    current_alarm.AlarmRaisedTime = new_event.EventTime;
                    current_alarm._event_type_id = alarm_type_id;
                    current_alarm._event_id = fault_mgmt._IndexEvent;
                }
                else
                    down_event_ref_count(current_alarm._event_id);

                if (action != ACTION_CLEAR)
                {
                    new_event.NotificationType = EVENT_NTFY_TYPE_OPT_CHANGED;
                    current_alarm._event_id = fault_mgmt._IndexEvent;
                    new_event._ref_count++;
                    save_current_alarm(i, &current_alarm);
                }
                else
                {
                    if (fault_mgmt.CurrentAlarmNumberOfEntries > 1)
                    {
                        /* Overwrite current alarm with the last item */
                        get_current_alarm(fault_mgmt.CurrentAlarmNumberOfEntries-1, &current_alarm_last);
                        save_current_alarm(i, &current_alarm_last);
                        put_current_alarm(&current_alarm_last);
                    }

                    remove_current_alarm(fault_mgmt.CurrentAlarmNumberOfEntries-1);
                    fault_mgmt.CurrentAlarmNumberOfEntries--;
                }
                /* If it is created by _get, we need to put */
                if (current_alarm_ready)
                    put_current_alarm(&current_alarm);

                break;
            }
            else
                continue;
        }
        else
        {
            if (action != ACTION_CLEAR)
            {
                /* MaxCurrentAlarmEntries is designed to be larger than the number of supported alarms
                 * so i >= MaxCurrentAlarmEntries should never happen*/
                new_event.NotificationType = EVENT_NTFY_TYPE_OPT_NEW;
                get_current_alarm(i, &current_alarm);
                current_alarm.AlarmRaisedTime = new_event.EventTime;
                current_alarm._event_type_id = alarm_type_id;
                current_alarm._event_id = fault_mgmt._IndexEvent;
                new_event._ref_count++;
                save_current_alarm(i, &current_alarm);

                fault_mgmt.CurrentAlarmNumberOfEntries++;
            }
            break;
        }
    }

    /* Add to history list */
    fault_mgmt._IndexHistoryEvent++;
    if (fault_mgmt._IndexHistoryEvent >= fault_mgmt.HistoryEventNumberOfEntries)
        fault_mgmt._IndexHistoryEvent = 0;
    if (get_event_referrer(fault_mgmt._IndexHistoryEvent, HISTORY_EVENT_NAME, &event_referrer) == SUCCESS)
        down_event_ref_count(event_referrer._event_id);

    new_event._ref_count++;
    event_referrer._event_id = fault_mgmt._IndexEvent;
    save_event_referrer(fault_mgmt._IndexHistoryEvent, HISTORY_EVENT_NAME, &event_referrer);

    /* Add to expedited list */
    if (strncmp(supported_alarm_item->ReportingMechanism, REPORT_MECHANISM_OPT_EXPEDITED, 1) == 0)
    {
        fault_mgmt._IndexExpeditedEvent++;
        if (fault_mgmt._IndexExpeditedEvent >= fault_mgmt.ExpeditedEventNumberOfEntries)
            fault_mgmt._IndexExpeditedEvent = 0;
        if (get_event_referrer(fault_mgmt._IndexExpeditedEvent, EXPEDITED_EVENT_NAME, &event_referrer) == SUCCESS)
            down_event_ref_count(event_referrer._event_id);

        new_event._ref_count++;
        event_referrer._event_id = fault_mgmt._IndexEvent;
        save_event_referrer(fault_mgmt._IndexExpeditedEvent, EXPEDITED_EVENT_NAME, &event_referrer);
    }

    /* Add to queued list */
    else if (strncmp(supported_alarm_item->ReportingMechanism, REPORT_MECHANISM_OPT_QUEUED, 1) == 0)
    {
        fault_mgmt._IndexQueuedEvent++;
        if (fault_mgmt._IndexQueuedEvent >= fault_mgmt.QueuedEventNumberOfEntries)
            fault_mgmt._IndexQueuedEvent = 0;
        if (get_event_referrer(fault_mgmt._IndexQueuedEvent, QUEUED_EVENT_NAME, &event_referrer) == SUCCESS)
            down_event_ref_count(event_referrer._event_id);

        new_event._ref_count++;
        event_referrer._event_id = fault_mgmt._IndexEvent;
        save_event_referrer(fault_mgmt._IndexQueuedEvent, QUEUED_EVENT_NAME, &event_referrer);
    }

    /* Save values to database */
    save_event(fault_mgmt._IndexEvent, &new_event);
    save_fault_mgmt(&fault_mgmt);
    return SUCCESS;
}


/****************************************************
 *  User Functions
 ****************************************************/
int fault_mgmt_startup(void)
{
    int ret = 0;

    if (rdb_open(NULL, &session) != 0 || session == NULL)
        return FAILED_OPEN_RDB;

    if (get_rdb_lock() != 0)
    {
        return FAILED_LOCK_RDB;
    }

    ret = get_supported_alarms(&alarm, &alarm_info, &alarm_count);

    put_rdb_lock();
    return ret;
}

int fault_mgmt_shutdown(void)
{
    if (alarm && alarm_count > 0)
        free_supported_alarms(&alarm, &alarm_info, &alarm_count);

    // close_fm_lock();

    if (session)
    {
        rdb_close(&session);
        session = NULL;
    }
    return SUCCESS;
}

int fault_mgmt_log(int alarm_type_id, const char *additional_text, const char *additional_info)
{
    int ret;

    if (!session)
    {
        return FAILED_FM_UNINITIALIZED;
    }

    if (get_rdb_lock() != 0)
    {
        return FAILED_LOCK_RDB;
    }

    ret = do_fault_mgmt_log(alarm_type_id, ACTION_SET, additional_text, additional_info);

    put_rdb_lock();
    return ret;
}

int fault_mgmt_clear(int alarm_type_id, const char *additional_text, const char *additional_info)
{
    int ret;

    if (!session)
    {
        return FAILED_FM_UNINITIALIZED;
    }

    if (get_rdb_lock() != 0)
    {
        return FAILED_LOCK_RDB;
    }

    ret = do_fault_mgmt_log(alarm_type_id, ACTION_CLEAR, additional_text, additional_info);

    put_rdb_lock();
    return ret;
}

int fault_mgmt_check_status(int alarm_type_id)
{
    int ret;

    if (!session)
    {
        return FAILED_FM_UNINITIALIZED;
    }

    if (get_rdb_lock() != 0)
    {
        return FAILED_LOCK_RDB;
    }

    ret = check_alarm_status(alarm_type_id);

    put_rdb_lock();
    return ret;
}

#if DEBUG
/**
 * @brief      This function writes to/read from RDB intensively in order to check
 *             if RDB works OK. This is only enabled under DEBUG mode.
 *
 * @return     0    - Passed.
 *             !0   - Failed.
 */
int fault_mgmt_test(void)
{
    int loop = 10000, i, ret;
    FaultMgmt fault_mgmt, fault_mgmt_read;

    while (loop)
    {
        fprintf(stderr, "============ Loop %d ============\n", loop);

        if ((ret = fault_mgmt_startup()) != SUCCESS)
        {
            fprintf(stderr, "Loop %d skipped for fault_mgmt_startup() failure (ret=%d)!\n", loop, ret);
            continue;
        }

        if (get_rdb_lock() != 0)
        {
            fprintf(stderr, "Loop %d skipped for lock failure!\n", loop);
            continue;
        }
        fprintf(stderr, "1\n");
        memset(&fault_mgmt, 0, sizeof(FaultMgmt));
        fprintf(stderr, "2\n");
        fault_mgmt.SupportedAlarmNumberOfEntries = loop;
        fault_mgmt.MaxCurrentAlarmEntries = loop;
        fault_mgmt.CurrentAlarmNumberOfEntries = loop;
        fault_mgmt.HistoryEventNumberOfEntries = loop;
        fault_mgmt.ExpeditedEventNumberOfEntries = loop;
        fault_mgmt.QueuedEventNumberOfEntries = loop;
        fault_mgmt._MaxEventEntries = loop;
        fault_mgmt._IndexEvent = loop;
        fault_mgmt._IndexHistoryEvent = loop;
        fault_mgmt._IndexExpeditedEvent = loop;
        fault_mgmt._IndexQueuedEvent = loop;

        fprintf(stderr, "Saving...\n");
        save_fault_mgmt(&fault_mgmt);

        put_rdb_lock();
        fault_mgmt_shutdown();

        for (i = 0; i < 10; i++)
        {
            memset(&fault_mgmt_read, 0, sizeof(FaultMgmt));

            if ((ret = fault_mgmt_startup()) != 0)
            {
                fprintf(stderr, "fault_mgmt_startup failed on loop %d (ret = %d)!\n", i, ret);
                continue;
            }

            fprintf(stderr, "Reading %d...\n", i);
            if ((ret = read_fault_mgmt(&fault_mgmt_read)) != 0)
            {
                fprintf(stderr, "Read failed on loop %d (ret = %d)!\n", i, ret);
                continue;
            }

            if (memcmp(&fault_mgmt, &fault_mgmt_read, sizeof(FaultMgmt)) != 0)
            {
                fprintf(stderr, "Read result unmatch on loop %d!\n", i);
                return -1;
            }

            fault_mgmt_shutdown();
        }

        fprintf(stderr, "Loop %d OK!\n", loop);
        loop--;
    }
    return 0;
}
#endif
