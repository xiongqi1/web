/***************************************************************************
 *    Copyright (C) 2004-2011 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/
/*
 * traceroute_profile.h
 *
 *  Created on: May 12, 2011
 *      Author: gonchar
 */

#ifndef traceroute_profile_H_
#define traceroute_profile_H_

#include "parameter.h"
#include "paramaccess.h"


// TraceRoute Profile. Getters:
int get_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_ResponseTime(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHopsNumberOfEntries(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHost(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopHostAddress(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopErrorCode(const char *name, ParameterType type, ParameterValue *value);
int get_InternetGatewayDevice_TraceRouteDiagnostics_RouteHops_i_HopRTTimes(const char *name, ParameterType type, ParameterValue *value);


// TraceRoute Profile. Setters:
int set_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_Interface(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_Host(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_NumberOfTries(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_Timeout(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_DataBlockSize(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_DSCP(const char *name, ParameterType type, ParameterValue *value);
int set_InternetGatewayDevice_TraceRouteDiagnostics_MaxHopCount(const char *name, ParameterType type, ParameterValue *value);

#endif /* traceroute_profile_H_ */
