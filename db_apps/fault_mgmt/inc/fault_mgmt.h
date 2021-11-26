/*
 * fault_mgmt.h
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system provides a solution to report system faults through TR-069.
 *
 */
#ifndef __FAULT_MGMT_H_16161623012018
#define __FAULT_MGMT_H_16161623012018

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief      Start up FaultMgmt system. This function needs to be called before
 *             any other functions are called.
 *
 * @return     0        - SUCCESS
 *             Others   - ERROR
 */
int fault_mgmt_startup(void);

/**
 * @brief      Shut down FaultMgmt system. This function needs to be called before
 *             application exits. FaultMgmt cannot be used after this function is
 *             called
 *
 * @return     Always 0 - SUCCESS
 */
int fault_mgmt_shutdown(void);

/**
 * @brief      Log/update an alarm.
 *
 * @param      alarm_type_id    The SupportedAlarm ID
 * @param      additional_text  The additional text
 * @param      additional_info  The additional information
 *
 * @return     0        - SUCCESS
 *             Others   - ERROR
 */
int fault_mgmt_log(int alarm_type_id, const char *additional_text, const char *additional_info);

/**
 * @brief      Clear an alarm.
 *
 * @param      alarm_type_id    The SupportedAlarm ID
 * @param      additional_text  The additional text
 * @param      additional_info  The additional information
 *
 * @return     0        - SUCCESS
 *             Others   - ERROR
 */
int fault_mgmt_clear(int alarm_type_id, const char *additional_text, const char *additional_info);

/**
 * @brief      Check if an alarm has been set
 *
 * @param      alarm_type_id  The SupportedAlarm ID
 *
 * @return     0        - Not set
 *             1        - Set
 *             Others   - ERROR
 */
int fault_mgmt_check_status(int alarm_type_id);

#ifdef __cplusplus
}
#endif

#endif // __FAULT_MGMT_H_16161623012018
