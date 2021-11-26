/*
 * fm_event.h
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system event handling functions.
 *
 */
#ifndef __FAULT_MGMT_EVENT_H__
#define __FAULT_MGMT_EVENT_H__

#include "fm_supported_alarm.h"

/****************************************************
 *  RDB Definitions
 ****************************************************/
enum EventParameter {
    EventTime = 0,
    AlarmIdentifier,
    NotificationType,
    ManagedObjectInstance,
    PerceivedSeverity_e,
    AdditionalText,
    AdditionalInformation,
    _ref_count,
    _event_type_id_e,
    EventParameterCount,
};

extern const char *EVENT_PARA_NAME[];
#define EVENT_PARA_NAME_DEF     { \
    [EventTime]                 = "EventTime", \
    [AlarmIdentifier]           = "AlarmIdentifier", \
    [NotificationType]          = "NotificationType", \
    [ManagedObjectInstance]     = "ManagedObjectInstance", \
    [PerceivedSeverity_e]       = "PerceivedSeverity", \
    [AdditionalText]            = "AdditionalText", \
    [AdditionalInformation]      = "AdditionalInformation", \
    [_ref_count]                = "_ref_count", \
    [_event_type_id_e]          = "_event_type_id", \
}

extern const int EVENT_MAX_STR_LEN[];
#define EVENT_MAX_STR_LEN_DEF   { \
    [EventTime]                 = 32, \
    [AlarmIdentifier]           = 65, \
    [NotificationType]          = 32, \
    [ManagedObjectInstance]     = 512, \
    [PerceivedSeverity_e]       = 64, \
    [AdditionalText]            = 256, \
    [AdditionalInformation]      = 256, \
    [_ref_count]                = 16, \
    [_event_type_id_e]          = 16, \
 }

#define EVENT_SEVERITY_OPT_CRITICAL         SEVERITY_OPT_CRITICAL
#define EVENT_SEVERITY_OPT_MAJOR            SEVERITY_OPT_MAJOR
#define EVENT_SEVERITY_OPT_MINOR            SEVERITY_OPT_MINOR
#define EVENT_SEVERITY_OPT_WARNING          SEVERITY_OPT_WARNING
#define EVENT_SEVERITY_OPT_INDETERMINATE    SEVERITY_OPT_INDETERMINATE

#define EVENT_NTFY_TYPE_OPT_NEW             "NewAlarm"
#define EVENT_NTFY_TYPE_OPT_CHANGED         "ChangedAlarm"
#define EVENT_NTFY_TYPE_OPT_CLEARED         "ClearedAlarm"


enum EventReferrerParameter {
    _event_id,
    EventReferrerParameterCount,
};

extern const char *EVENT_REFERRER_PARA_NAME[];
#define EVENT_REFERRER_PARA_NAME_DEF     { \
    [_event_id]                 = "_event_id", \
}

#define HISTORY_EVENT_NAME      "HistoryEvent"
#define EXPEDITED_EVENT_NAME    "ExpeditedEvent"
#define QUEUED_EVENT_NAME       "QueuedEvent"


/****************************************************
 *  Implementation Object Definitions
 ****************************************************/
typedef struct _Event {
    char *EventTime;
    char *AlarmIdentifier;
    char *NotificationType;
    char *ManagedObjectInstance;
    char *PerceivedSeverity;
    char *AdditionalText;
    char *AdditionalInformation;
    int _ref_count;
    int _event_type_id;
} Event;


typedef struct _EventReferrer {
    int _event_id;
} EventReferrer;


/****************************************************
 *  Event Functions
 ****************************************************/
/**
 * @brief      Read event object from RDB. Please note new space will be allocated
 *             for storing data, so event objects should be put by calling
 *             put_event() after usage.
 *
 * @param      idx    The event object index
 * @param      event  The event object to save to
 *
 * @return     SUCCESS or FAILED_GET_PARA
 */
int get_event(int idx, Event *event);

/**
 * @brief      Puts an event object that was get by get_event. This function frees
 *             any memory that is allocated by get_event function.
 *
 * @param      event  The event object to free
 */
void put_event(Event *event);

/**
 * @brief      Saves an event object into RDB.
 *
 * @param      idx    The RDB instance index to save to
 * @param      event  The event object that contains the info to save
 */
void save_event(int idx, const Event *event);


/**
 * @brief      Gets the event reference count.
 *
 * @param      idx   The event object index
 *
 * @return     The event reference count.
 */
int get_event_ref_count(int idx);

/**
 * @brief      Increase event ref count by 1
 *
 * @param      idx   The event object index
 *
 * @return     The new ref count
 */
int up_event_ref_count(int idx);

/**
 * @brief      Decrease event ref count by 1
 *
 * @param      idx   The event object index
 *
 * @return     The new ref count
 */
int down_event_ref_count(int idx);


/****************************************************
 *  Event Referrer (History/Expedited/Queued) Functions
 ****************************************************/
/**
 * @brief      Gets an event referrer object that refers to an event object. No
 *             new space is allocated by this function.
 *
 * @param      idx             The index of the referrer object
 * @param      name            The name of the referrer object
 * @param[OUT] event_referrer  The event referrer object to read to
 *
 * @return     The event referrer.
 */
int get_event_referrer(int idx, const char *name, EventReferrer *event_referrer);

/**
 * @brief      Save an event referrer object to RDB object
 *
 * @param      idx             The index of the referrer object
 * @param      name            The name of the referrer object
 * @param      event_referrer  The event referrer object that contains info to save
 */
void save_event_referrer(int idx, const char *name, const EventReferrer *event_referrer);

#endif // __FAULT_MGMT_EVENT_H__
