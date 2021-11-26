/*
 * fm_current_alarm.h
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system CurrentAlarm object handling functions.
 *
 */
#ifndef __FAULT_MGMT_CURRENT_ALARM_H__
#define __FAULT_MGMT_CURRENT_ALARM_H__

/****************************************************
 *  RDB Definitions
 ****************************************************/
enum CurrentAlarmParameter {
    AlarmRaisedTime = 0,
    _event_id_ca,
    _event_type_id_ca,
    CurrentAlarmParameterCount,
};

extern const char *CURRENT_ALARM_PARA_NAME[];
#define CURRENT_ALARM_PARA_NAME_DEF     { \
    [AlarmRaisedTime]           = "AlarmRaisedTime", \
    [_event_id_ca]              = "_event_id", \
    [_event_type_id_ca]         = "_event_type_id", \
}


/****************************************************
 *  Implementation Object Definitions
 ****************************************************/
typedef struct _CurrentAlarm
{
    char *AlarmRaisedTime;
    int _event_id;
    int _event_type_id;
} CurrentAlarm;


/****************************************************
 *  Functions
 ****************************************************/
/**
 * @brief      Gets the current alarm from RDB. Allocate any necessary space to
 *             store the information. The object has to be put back by calling
 *             put_current_alarm() to release allocated memory.
 *
 * @param      idx            The index of current alarm
 * @param      current_alarm  The current alarm to save the loadded info
 *
 * @return     SUCCESS or FAILED_GET_PARA
 */
int get_current_alarm(int idx, CurrentAlarm *current_alarm);

/**
 * @brief      Puts a current alarm object. This function releases any space
 *             allocated in get_current_alarm()
 *
 * @param      current_alarm  The current alarm
 */
void put_current_alarm(CurrentAlarm *current_alarm);

/**
 * @brief      Saves a current alarm into RDB at the given index.
 *
 * @param      idx            The index of current alarm to write on.
 * @param      current_alarm  The current alarm object to save
 */
void save_current_alarm(int idx, CurrentAlarm *current_alarm);

/**
 * @brief      Removes a current alarm from rdb.
 *
 * @param      idx   The index of current alarm
 */
void remove_current_alarm(int idx);

/**
 * @brief      Gets the current alarm event type identifier.
 *
 * @param      idx   The index of current alarm
 *
 * @return     When idx >= 0, The current alarm event type identifier.
 *             When idx < 0,  Error occurred.
 */
int get_current_alarm_event_type_id(int idx);

#endif // __FAULT_MGMT_CURRENT_ALARM_H__
