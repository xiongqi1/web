/*
 * fm_current_alarm.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system Event-related objects handling functions.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "fm_errno.h"
#include "fm_shared.h"
#include "fm_event.h"

const char *EVENT_PARA_NAME[] = EVENT_PARA_NAME_DEF;
const int EVENT_MAX_STR_LEN[] = EVENT_MAX_STR_LEN_DEF;
const char *EVENT_REFERRER_PARA_NAME[] = EVENT_REFERRER_PARA_NAME_DEF;

int get_event_ref_count(int idx)
{
    char szname[256];
    assert(session != NULL);

    if (idx < 0)
    {
        return FAILED_PARA_NOT_EXIST;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[_ref_count]);
    return get_number(szname);
}

/**
 * @brief      Sets the event reference count.
 *
 * @param      idx        The event object index
 * @param      ref_count  The reference count
 *
 * @return     SUCCESS or FAILED_SET_PARA
 */
int set_event_ref_count(int idx, int ref_count)
{
    char szname[256];
    assert(session != NULL);

    if (idx < 0)
    {
        return FAILED_SET_PARA;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[_ref_count]);
    return set_number(szname, ref_count);
}

int up_event_ref_count(int idx)
{
    int count = get_event_ref_count(idx);
    if (count >= 0)
    {
        set_event_ref_count(idx, ++count);
    }
    return count;
}

int down_event_ref_count(int idx)
{
    int count = get_event_ref_count(idx);
    if (count > 0)
    {
        set_event_ref_count(idx, --count);
    }
    return count;
}

int get_event(int idx, Event *event)
{
    char szname[256];
    int len, i, ret = SUCCESS;
    char *parameters[EventParameterCount] = {NULL};
    assert(session != NULL);

    if (idx < 0)
    {
        return FAILED_GET_PARA;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    for (i = 0; i < EventParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[i]);
        len = 0;
        if (rdb_get_alloc(session, szname, &parameters[i], &len) != 0)
        {
            ret = FAILED_GET_PARA;
            goto exit;
        }
    }

    /* If event is NULL, we just return if this event item exists */
    if (!event)
        goto exit;

    event->EventTime = parameters[EventTime];
    event->AlarmIdentifier = parameters[AlarmIdentifier];
    event->NotificationType = parameters[NotificationType];
    event->ManagedObjectInstance = parameters[ManagedObjectInstance];
    event->PerceivedSeverity = parameters[PerceivedSeverity_e];
    event->AdditionalText = parameters[AdditionalText];
    event->AdditionalInformation = parameters[AdditionalInformation];
    event->_ref_count = atoi(parameters[_ref_count]);
    free(parameters[_ref_count]);
    event->_event_type_id = atoi(parameters[_event_type_id_e]);
    free(parameters[_event_type_id_e]);
    return ret;

exit:
    for (i = 0; i < EventParameterCount; i++)
        safe_free(parameters[i]);
    return ret;
}

void put_event(Event *event)
{
    assert(event != NULL);
    safe_free(event->EventTime);
    safe_free(event->AlarmIdentifier);
    safe_free(event->NotificationType);
    safe_free(event->ManagedObjectInstance);
    safe_free(event->PerceivedSeverity);
    safe_free(event->AdditionalText);
    safe_free(event->AdditionalInformation);
}


void save_event(int idx, const Event *event)
{
    char szname[256], tmp_val[16];
    assert(session != NULL);

    if (idx < 0 || !event)
    {
        return;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[EventTime]);
    add_or_update(szname, event->EventTime, strlen(event->EventTime) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[AlarmIdentifier]);
    add_or_update(szname, event->AlarmIdentifier, strlen(event->AlarmIdentifier) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[NotificationType]);
    add_or_update(szname, event->NotificationType, strlen(event->NotificationType) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[ManagedObjectInstance]);
    add_or_update(szname, event->ManagedObjectInstance, strlen(event->ManagedObjectInstance) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[PerceivedSeverity_e]);
    add_or_update(szname, event->PerceivedSeverity, strlen(event->PerceivedSeverity) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[AdditionalText]);
    add_or_update(szname, event->AdditionalText, strlen(event->AdditionalText) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[AdditionalInformation]);
    add_or_update(szname, event->AdditionalInformation, strlen(event->AdditionalInformation) + 1, PERSIST_FLAG_EVENT);

    sprintf(tmp_val, "%d", event->_ref_count);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[_ref_count]);
    add_or_update(szname, tmp_val, strlen(tmp_val) + 1, PERSIST_FLAG_EVENT);

    sprintf(tmp_val, "%d", event->_event_type_id);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.%s", idx, EVENT_PARA_NAME[_event_type_id_e]);
    add_or_update(szname, tmp_val, strlen(tmp_val) + 1, PERSIST_FLAG_EVENT);

    add_object_id_to_index(FAULT_MGMT_DB_PATH_EVENT FAULT_MGMT_DB_INDEX_SUFFIX, idx, PERSIST_FLAG_EVENT);
}


int get_event_referrer(int idx, const char *name, EventReferrer *event_referrer)
{
    char szname[256], tmp_val[16];
    int len = sizeof(tmp_val);
    assert(session != NULL);

    if (idx < 0)
    {
        return FAILED_GET_PARA;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH ".%s.%d.%s", name, idx, EVENT_REFERRER_PARA_NAME[_event_id]);
    if (rdb_get(session, szname, tmp_val, &len) != 0)
        return FAILED_GET_PARA;

    if (event_referrer)
    {
        /* Offset ID by -1 as we added 1 to be compatible with LUA TR-069 lib */
        event_referrer->_event_id = atoi(tmp_val) - 1;
    }
    return SUCCESS;
}

void save_event_referrer(int idx, const char *name, const EventReferrer *event_referrer)
{
    char szname[256], tmp_val[16];
    assert(session != NULL);

    if (idx < 0 || !event_referrer)
    {
        return;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH ".%s.%d.%s", name, idx, EVENT_REFERRER_PARA_NAME[_event_id]);
    /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
    sprintf(tmp_val, "%d", event_referrer->_event_id + 1);
    add_or_update(szname, tmp_val, strlen(tmp_val) + 1, PERSIST_FLAG_EVENT);

    sprintf(szname, FAULT_MGMT_DB_PATH ".%s" FAULT_MGMT_DB_INDEX_SUFFIX, name);
    add_object_id_to_index(szname, idx, PERSIST_FLAG_EVENT);
}