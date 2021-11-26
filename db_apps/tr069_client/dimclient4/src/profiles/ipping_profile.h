/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/
/*
 * ipping_profile.h
 *
 *  Created on: May 6, 2011
 *      Author: San
 */

#ifndef ipping_profile_H_
#define ipping_profile_H_

#include "paramaccess.h"
#include "parameter.h"

// IPPing Profile. Getters:
int get_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_SuccessCount(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_FailureCount(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_AverageResponseTime(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_MinimumResponseTime(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_IPPingDiagnostics_MaximumResponseTime(const char *name, ParameterType type, ParameterValue *value);

// IPPing Profile. Setters:
int set_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_IPPingDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_IPPingDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_IPPingDiagnostics_NumberOfRepetitions(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_IPPingDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_IPPingDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_IPPingDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value);
#endif /* ipping_profile_H_ */
