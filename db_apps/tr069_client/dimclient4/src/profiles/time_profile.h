/***************************************************************************
 *    Copyright (C) 2004-2011 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/
/*
 * time_profile.h
 *
 *  Created on: May 12, 2011
 *      Author: gonchar
 */

#ifndef time_profile_H_
#define time_profile_H_

#include "parameter.h"
#include "paramaccess.h"


// Time Profile. Getters:
int get_InternetGatewayDevice_Time_Enable(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_Status(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_NTPServer1(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_NTPServer2(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_NTPServer3(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_NTPServer4(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_NTPServer5(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_CurrentLocalTime(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_LocalTimeZone(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_LocalTimeZoneName(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_DaylightSavingsUsed(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_DaylightSavingsStart(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_Time_DaylightSavingsEnd(const char *name, ParameterType type, ParameterValue *value);


// Time Profile. Setters:
int set_InternetGatewayDevice_Time_Enable(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_NTPServer1(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_NTPServer2(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_NTPServer3(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_NTPServer4(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_NTPServer5(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_LocalTimeZone(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_LocalTimeZoneName(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_DaylightSavingsUsed(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_DaylightSavingsStart(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_Time_DaylightSavingsEnd(const char *name, ParameterType type, ParameterValue *value);

#endif /* time_profile_H_ */
