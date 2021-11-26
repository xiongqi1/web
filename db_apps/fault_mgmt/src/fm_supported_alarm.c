/*
 * fm_supported_alarm.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system SupportedAlarm object handling functions.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "rdb_ops.h"
#include "fm_errno.h"
#include "fm_shared.h"
#include "fm_supported_alarm.h"

#define SUPPORTED_ALARM_INDEX_KEY FAULT_MGMT_DB_PATH_SUPPORTED_ALARM FAULT_MGMT_DB_INDEX_SUFFIX

extern struct rdb_session *session;

const char *ALARM_PARA_NAME[] = ALARM_PARA_NAME_DEF;
const int ALARM_PARA_MAX_STR_LEN[] = ALARM_PARA_MAX_STR_LEN_DEF;


/****************************************************
 *  Local Functions
 ****************************************************/
static int _add_supported_alarm(int id, const char *parameters[SupportedAlarmParameterCount])
{
    int i, len;
    char szname[256];
    int ret = SUCCESS;

    assert(session != NULL && parameters != NULL);

    if (id < 0)
    {
        return FAILED_CREATE_OBJECT;
    }

    /* Add or set parameters */
    for (i = 0; i < SupportedAlarmParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.%s", id, ALARM_PARA_NAME[i]);
        len = strlen(parameters[i]) + 1;
        len = (len <= ALARM_PARA_MAX_STR_LEN[i])? len : ALARM_PARA_MAX_STR_LEN[i];
        if ((ret = add_or_update(szname, parameters[i], len, PERSIST_FLAG_SUPPORTED_ALARM)) != SUCCESS)
            return ret;
    }

    /* Add to index */
    add_object_id_to_index(SUPPORTED_ALARM_INDEX_KEY, id, PERSIST_FLAG_SUPPORTED_ALARM);
    return ret;
}

static int _get_supported_alarm(int id, char *parameters[SupportedAlarmParameterCount])
{
    int i, len;
    char szname[256];
    int ret = SUCCESS;

    assert(session != NULL);

    if (id < 0)
        return FAILED_INVALID_PARA;

    for (i = 0; i < SupportedAlarmParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.%s", id, ALARM_PARA_NAME[i]);
        len = 0;
        if ((ret = rdb_get_alloc(session, szname, &parameters[i], &len)) != SUCCESS)
        {
            if (parameters[i] == NULL)
            {
                parameters[i] = malloc(1);
                parameters[i][0] = '\0';
            }
            continue;
        }
    }
    return SUCCESS;
}

static int _remove_supported_alarm(int id)
{
    int ret = 0, i, r;
    char szname[256];

    assert(session != NULL && id >= 0);

    /* Delete parameters */
    for (i = 0; i < SupportedAlarmParameterCount; i++)
    {
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.%s", id, ALARM_PARA_NAME[i]);
        /* rdb_getinfo returns -EOVERFLOW even if it succeeds */
        r = rdb_getinfo(session, szname, NULL, NULL, NULL);
        if (r == -EOVERFLOW || r == 0)
        {
            ret += rdb_delete(session, szname);
        }
        else
        {
            ret += r;
        }
    }

#if defined(ADD_EMPTY_PARENT_OBJ) && ADD_EMPTY_PARENT_OBJ
    /* Try deleting the master object */
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d", id);
    r = rdb_getinfo(session, szname, NULL, NULL, NULL);
    /* rdb_getinfo returns -EOVERFLOW even if it succeeds */
    if (r == -EOVERFLOW || r == 0)
    {
        ret += rdb_delete(session, szname);
    }
    else
    {
        ret += r;
    }
#endif

    /* Remove from index */
    remove_object_id_from_index(SUPPORTED_ALARM_INDEX_KEY, id, PERSIST_FLAG_SUPPORTED_ALARM);

    if (ret == 0)
        return SUCCESS;
    else
        return FAILED_DELETE_OBJECT;
    return SUCCESS;
}


/****************************************************
 *  Exposed Functions
 ****************************************************/
int add_supported_alarm(const SupportedAlarmInfo *alarm_info)
{
    int i, ret;
    int new_open = 0;

    if (!session)
    {
        if (rdb_open(NULL, &session) != 0 || session == NULL)
            return FAILED_OPEN_RDB;

        new_open = 1;
        if (get_rdb_lock() != 0)
        {
            rdb_close(&session);
            return FAILED_LOCK_RDB;
        }
    }

    if (!alarm_info)
        return FAILED_INVALID_PARA;

    for (i = 0; i < SupportedAlarmParameterCount; i++)
        if (!alarm_info->parameters[i])
            return FAILED_INVALID_PARA;

    ret = _add_supported_alarm(alarm_info->id, (const char **)alarm_info->parameters);

    if (new_open)
    {
        put_rdb_lock();
        rdb_close(&session);
    }
    return ret;
}

int remove_supported_alarm(int id)
{
    int ret;
    int new_open = 0;

    if (!session)
    {
        if (rdb_open(NULL, &session) != 0 || session == NULL)
            return FAILED_OPEN_RDB;

        new_open = 1;
        if (get_rdb_lock() != 0)
        {
            rdb_close(&session);
            return FAILED_LOCK_RDB;
        }
    }

    if (id < 0)
        return FAILED_INVALID_PARA;

    ret = _remove_supported_alarm(id);

    if (new_open)
    {
        put_rdb_lock();
        rdb_close(&session);
    }
    return ret;
}

int get_supported_alarms_count(void)
{
    int ret = 0, new_open = 0;

    if (!session)
    {
        if (rdb_open(NULL, &session) != 0 || session == NULL)
            return FAILED_OPEN_RDB;

        new_open = 1;
        if (get_rdb_lock() != 0)
        {
            rdb_close(&session);
            return FAILED_LOCK_RDB;
        }
    }

    ret = count_object_id_in_index(SUPPORTED_ALARM_INDEX_KEY);

    if (new_open)
    {
        put_rdb_lock();
        rdb_close(&session);
    }
    return ret;
}

int get_supported_alarms(SupportedAlarm **alarms[], SupportedAlarmInfo **alarm_info[], int *count)
{
    SupportedAlarmInfo **tmp_alarm_info = NULL;
    int new_open = 0;
    char *str_index = NULL, *str_pos = NULL;
    int ret = 0, id_count = 0, tmp_id = -1, len = 0, i = 0;
    char id_str[16], *id_str_pos = NULL;

    if (!alarms || !count)
        return FAILED_INVALID_PARA;

    if (!session)
    {
        if (rdb_open(NULL, &session) != 0 || session == NULL)
            return FAILED_OPEN_RDB;

        new_open = 1;
        if (get_rdb_lock() != 0)
        {
            rdb_close(&session);
            return FAILED_LOCK_RDB;
        }
    }

    if ((id_count = count_object_id_in_index(SUPPORTED_ALARM_INDEX_KEY)) <= 0)
    {
        *count = 0;
        ret = id_count;
        goto exit;
    }

    /* Read info into tmp_alarm_info */
    tmp_alarm_info = calloc(sizeof(SupportedAlarmInfo *), id_count);

    /* This should succeed if count_object_id_in_index succeeded */
    rdb_get_alloc(session, SUPPORTED_ALARM_INDEX_KEY, &str_index, &len);
    if (str_index != NULL && len > 0)
    {
        str_pos = str_index;
        id_str_pos = &id_str[0];
        while(1)
        {
            if (isdigit(*str_pos) || (id_str_pos == &id_str[0] && *str_pos == '-'))
            {
                *id_str_pos++ = *str_pos;
            }
            else if (*str_pos == ',' || *str_pos == '\0')
            {
                *id_str_pos = '\0';
                if (id_str_pos != &id_str[0] && i < id_count)
                {
                    tmp_id = atoi(id_str);
                    tmp_alarm_info[i] = calloc(sizeof(SupportedAlarmInfo), 1);
                    tmp_alarm_info[i]->id = tmp_id;
                    if (_get_supported_alarm(tmp_id, tmp_alarm_info[i]->parameters) != SUCCESS)
                    {
                        /* Skip the invalid item */
                        id_count--;
                    }
                    else
                    {
                        i++;
                    }
                }
                id_str_pos = &id_str[0];
            }

            if (*str_pos == '\0')
                break;
            else
                str_pos++;
        }

        tmp_alarm_info = realloc(tmp_alarm_info, sizeof(SupportedAlarmInfo *) * id_count);
        *count = id_count;

        /* Convert to SupportedAlarmObject */
        (*alarms) = calloc(sizeof(SupportedAlarm *), id_count);
        for (i = 0; i < id_count; i++)
        {
            (*alarms)[i] = calloc(sizeof(SupportedAlarm), 1);
            if (tmp_alarm_info[i])
            {
                (*alarms)[i]->PerceivedSeverity = tmp_alarm_info[i]->parameters[PerceivedSeverity];
                (*alarms)[i]->ReportingMechanism = tmp_alarm_info[i]->parameters[ReportingMechanism];
                (*alarms)[i]->EventType = tmp_alarm_info[i]->parameters[EventType];
                (*alarms)[i]->ProbableCause = tmp_alarm_info[i]->parameters[ProbableCause];
                (*alarms)[i]->SpecificProblem = tmp_alarm_info[i]->parameters[SpecificProblem];
                (*alarms)[i]->_flags = tmp_alarm_info[i]->parameters[_flags];
                (*alarms)[i]->_event_type_id = tmp_alarm_info[i]->id;
            }
        }
    }

exit:
    /* Free alarm info if it is not used */
    if (tmp_alarm_info)
    {
        if (alarm_info)
        {
            *alarm_info = tmp_alarm_info;
        }
        else
        {
            for (i = 0; i < id_count; i++)
                safe_free(tmp_alarm_info[i]);
            free(tmp_alarm_info);
        }
    }

    if (new_open)
    {
        put_rdb_lock();
        rdb_close(&session);
    }
    return ret;
}

int free_supported_alarms(SupportedAlarm **alarms[], SupportedAlarmInfo **alarm_info[], int *count)
{
    int i;
    if (!alarms || !(*alarms) || !count)
        return 0;

    for (i = 0; i < *count; i++)
    {
        if (!(*alarms)[i])
            continue;

        safe_free((*alarms)[i]->PerceivedSeverity);
        safe_free((*alarms)[i]->ReportingMechanism);
        safe_free((*alarms)[i]->EventType);
        safe_free((*alarms)[i]->ProbableCause);
        safe_free((*alarms)[i]->SpecificProblem);
        safe_free((*alarms)[i]->_flags);
        free((*alarms)[i]);
    }
    safe_free(*alarms);

    if (alarm_info && *alarm_info)
    {
        for (i = 0; i < *count; i++)
            safe_free((*alarm_info)[i]);
        safe_free(*alarm_info);
    }

    *count = 0;
    return 0;
}