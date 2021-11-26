/*
 * fm_shared.h
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * Shared definitions within FaultMgmt system.
 *
 */

#ifndef __FAULT_MGMT_SHARED_H__
#define __FAULT_MGMT_SHARED_H__

#include "rdb_ops.h"

/* Decide if events are logged persistantly, use 0 or PERSIST */
#define PERSIST_FLAG_SUPPORTED_ALARM            0
#define PERSIST_FLAG_EVENT                      0

#define FAULT_MGMT_DB_PATH                      "tr069.FaultMgmt"
#define FAULT_MGMT_DB_PATH_SUPPORTED_ALARM_NUM  FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries"
#define FAULT_MGMT_DB_PATH_MAX_ALARM_ENTRY      FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries"
#define FAULT_MGMT_DB_PATH_SUPPORTED_ALARM      FAULT_MGMT_DB_PATH ".SupportedAlarm"
#define FAULT_MGMT_DB_PATH_EVENT                FAULT_MGMT_DB_PATH ".Event"
#define FAULT_MGMT_DB_PATH_CURRENT_ALARM        FAULT_MGMT_DB_PATH ".CurrentAlarm"
#define FAULT_MGMT_DB_PATH_HISTORY_EVENT        FAULT_MGMT_DB_PATH ".HistoryEvent"
#define FAULT_MGMT_DB_PATH_EXPEDITED_EVENT      FAULT_MGMT_DB_PATH ".ExpeditedEvent"
#define FAULT_MGMT_DB_PATH_QUEUED_EVENT         FAULT_MGMT_DB_PATH ".QueuedEvent"

#define FAULT_MGMT_DB_INDEX_SUFFIX              "._index"


extern struct rdb_session *session;


#define safe_free(x)    do { free(x); x = NULL; } while (0)


/****************************************************
 *  RDB Support Functions
 ****************************************************/
int get_rdb_lock(void);
int put_rdb_lock(void);
int add_or_update(const char *szname, const char *value, int len, int flag);
int get_number(const char * path);
int set_number(const char * path, int number);
void add_object_id_to_index(const char *path, int id, int persist);
void remove_object_id_from_index(const char *path, int id, int persist);
int count_object_id_in_index(const char *path);

/****************************************************
 *  Utility Functions
 ****************************************************/
int count_index(const char *str_index);
int find_index(const char *str_index, int id, int *start, int *end);
int add_index(char **str_index, int id);
int remove_index(char **str_index, int id);

#endif // __FAULT_MGMT_SHARED_H__
