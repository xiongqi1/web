/*
 * fm_current_alarm.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system CurrentAlarm object handling functions.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "rdb_ops.h"
#include "fm_errno.h"
#include "fm_shared.h"
#include "fm_current_alarm.h"

const char *CURRENT_ALARM_PARA_NAME[] = CURRENT_ALARM_PARA_NAME_DEF;

int get_current_alarm_event_type_id(int idx)
{
    char szname[256], tmp_val[16];
    int len = sizeof(tmp_val);
    assert(session != NULL);

    if (idx < 0)
    {
        return -1;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[_event_type_id_ca]);
    if (rdb_get(session, szname, tmp_val, &len) != 0)
        return -1;
    else
        return atoi(tmp_val);
}

int get_current_alarm(int idx, CurrentAlarm *current_alarm)
{
    char szname[256], tmp_val[16];
    int len = sizeof(tmp_val), i, ret = SUCCESS;
    char *parameters[CurrentAlarmParameterCount] = {NULL};
    assert(session != NULL && current_alarm != NULL);

    if (idx < 0)
    {
        return FAILED_GET_PARA;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    for (i = 0; i < CurrentAlarmParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[i]);
        len = 0;
        if (rdb_get_alloc(session, szname, &parameters[i], &len) != 0)
        {
            ret = FAILED_GET_PARA;
            goto fault_exit;
        }
    }

    current_alarm->AlarmRaisedTime = parameters[AlarmRaisedTime];
    /* Offset event ID by +1 so it is compatible with LUA TR-069 lib */
    current_alarm->_event_id = atoi(parameters[_event_id_ca]) - 1;
    free(parameters[_event_id_ca]);
    current_alarm->_event_type_id = atoi(parameters[_event_type_id_ca]);
    free(parameters[_event_type_id_ca]);

    return ret;

fault_exit:
    for (i = 0; i < CurrentAlarmParameterCount; i++)
        safe_free(parameters[i]);
    return ret;
}

void put_current_alarm(CurrentAlarm *current_alarm)
{
    assert(current_alarm != NULL);

    safe_free(current_alarm->AlarmRaisedTime);
}

void save_current_alarm(int idx, CurrentAlarm *current_alarm)
{
    char szname[256], tmp_val[16];
    assert(session != NULL && current_alarm != NULL);

    if (idx < 0)
    {
        return;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[AlarmRaisedTime]);
    add_or_update(szname, current_alarm->AlarmRaisedTime, strlen(current_alarm->AlarmRaisedTime) + 1, PERSIST_FLAG_EVENT);

    /* Offset event ID by +1 so it is compatible with LUA TR-069 lib */
    sprintf(tmp_val, "%d", current_alarm->_event_id + 1);
    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[_event_id_ca]);
    add_or_update(szname, tmp_val, strlen(tmp_val) + 1, PERSIST_FLAG_EVENT);

    sprintf(tmp_val, "%d", current_alarm->_event_type_id);
    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[_event_type_id_ca]);
    add_or_update(szname, tmp_val, strlen(tmp_val) + 1, PERSIST_FLAG_EVENT);

    add_object_id_to_index(FAULT_MGMT_DB_PATH_CURRENT_ALARM FAULT_MGMT_DB_INDEX_SUFFIX, idx, PERSIST_FLAG_EVENT);
}

void remove_current_alarm(int idx)
{
    char szname[256];
    assert(session != NULL);

    if (idx < 0)
    {
        return;
    }
    else
    {
        /* Offset current alarm ID by +1 so it is compatible with LUA TR-069 lib */
        idx++;
    }

    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[AlarmRaisedTime]);
    rdb_delete(session, szname);

    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[_event_id_ca]);
    rdb_delete(session, szname);

    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.%s", idx, CURRENT_ALARM_PARA_NAME[_event_type_id_ca]);
    rdb_delete(session, szname);

    remove_object_id_from_index(FAULT_MGMT_DB_PATH_CURRENT_ALARM FAULT_MGMT_DB_INDEX_SUFFIX, idx, PERSIST_FLAG_EVENT);
}