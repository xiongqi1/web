/*
 * fm_errno.h
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system error numbers.
 *
 */
#ifndef __FAULT_MGMT_ERRNO_H__
#define __FAULT_MGMT_ERRNO_H__

#define SUCCESS                             (0)
#define FAILED_INVALID_PARA                 (-1)
#define FAILED_OPEN_RDB                     (-2)
#define FAILED_LOCK_RDB                     (-3)
#define FAILED_GET_INFO                     (-4)
#define FAILED_CREATE_OBJECT                (-5)
#define FAILED_DELETE_OBJECT                (-6)
#define FAILED_CREATE_PARA                  (-7)
#define FAILED_SET_PARA                     (-8)
#define FAILED_GET_PARA                     (-9)
#define FAILED_FM_UNINITIALIZED             (-10)
#define FAILED_ALLOCATING_MEM               (-11)
#define FAILED_INVALID_ALARM_ID             (-12)
#define FAILED_PARA_NOT_EXIST               (-13)

#endif // __FAULT_MGMT_ERRNO_H__
