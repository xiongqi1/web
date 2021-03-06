/* soapClient.cpp
   Generated by gSOAP 2.7.0f from interop_all.h
   Copyright (C) 2001-2004 Genivia, Inc. All Rights Reserved.
   This software is released under one of the following three licenses:
   GPL, the gSOAP public license, or Genivia's license for commercial use.
   See README.txt for further details.
*/
#include "soapH.h"

SOAP_BEGIN_NAMESPACE(soap)

SOAP_SOURCE_STAMP("@(#) soapClient.cpp ver 2.7.0f 2005-03-02 16:50:00 GMT")


SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoString(struct soap *soap, const char *URL, const char *action, char *inputString, char *&_return)
{
	struct ns__echoString soap_tmp_ns__echoString;
	struct ns__echoStringResponse *soap_tmp_ns__echoStringResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoString.inputString=inputString;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoString(soap, &soap_tmp_ns__echoString);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoString(soap, &soap_tmp_ns__echoString, "ns:echoString", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoString(soap, &soap_tmp_ns__echoString, "ns:echoString", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	_return = NULL;
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoStringResponse = soap_get_ns__echoStringResponse(soap, NULL, "ns:echoStringResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoStringResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoStringArray(struct soap *soap, const char *URL, const char *action, struct ArrayOfstring inputStringArray, struct ArrayOfstring &_return)
{
	struct ns__echoStringArray soap_tmp_ns__echoStringArray;
	struct ns__echoStringArrayResponse *soap_tmp_ns__echoStringArrayResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoStringArray.inputStringArray=inputStringArray;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoStringArray(soap, &soap_tmp_ns__echoStringArray);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoStringArray(soap, &soap_tmp_ns__echoStringArray, "ns:echoStringArray", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoStringArray(soap, &soap_tmp_ns__echoStringArray, "ns:echoStringArray", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ArrayOfstring(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoStringArrayResponse = soap_get_ns__echoStringArrayResponse(soap, NULL, "ns:echoStringArrayResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoStringArrayResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoInteger(struct soap *soap, const char *URL, const char *action, long inputInteger, long &_return)
{
	struct ns__echoInteger soap_tmp_ns__echoInteger;
	struct ns__echoIntegerResponse *soap_tmp_ns__echoIntegerResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoInteger.inputInteger=inputInteger;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoInteger(soap, &soap_tmp_ns__echoInteger);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoInteger(soap, &soap_tmp_ns__echoInteger, "ns:echoInteger", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoInteger(soap, &soap_tmp_ns__echoInteger, "ns:echoInteger", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_xsd__int(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoIntegerResponse = soap_get_ns__echoIntegerResponse(soap, NULL, "ns:echoIntegerResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoIntegerResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoIntegerArray(struct soap *soap, const char *URL, const char *action, struct ArrayOfint inputIntegerArray, struct ArrayOfint &_return)
{
	struct ns__echoIntegerArray soap_tmp_ns__echoIntegerArray;
	struct ns__echoIntegerArrayResponse *soap_tmp_ns__echoIntegerArrayResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoIntegerArray.inputIntegerArray=inputIntegerArray;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoIntegerArray(soap, &soap_tmp_ns__echoIntegerArray);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoIntegerArray(soap, &soap_tmp_ns__echoIntegerArray, "ns:echoIntegerArray", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoIntegerArray(soap, &soap_tmp_ns__echoIntegerArray, "ns:echoIntegerArray", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ArrayOfint(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoIntegerArrayResponse = soap_get_ns__echoIntegerArrayResponse(soap, NULL, "ns:echoIntegerArrayResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoIntegerArrayResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoFloat(struct soap *soap, const char *URL, const char *action, float inputFloat, float &_return)
{
	struct ns__echoFloat soap_tmp_ns__echoFloat;
	struct ns__echoFloatResponse *soap_tmp_ns__echoFloatResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoFloat.inputFloat=inputFloat;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoFloat(soap, &soap_tmp_ns__echoFloat);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoFloat(soap, &soap_tmp_ns__echoFloat, "ns:echoFloat", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoFloat(soap, &soap_tmp_ns__echoFloat, "ns:echoFloat", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_xsd__float(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoFloatResponse = soap_get_ns__echoFloatResponse(soap, NULL, "ns:echoFloatResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoFloatResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoFloatArray(struct soap *soap, const char *URL, const char *action, struct ArrayOffloat inputFloatArray, struct ArrayOffloat &_return)
{
	struct ns__echoFloatArray soap_tmp_ns__echoFloatArray;
	struct ns__echoFloatArrayResponse *soap_tmp_ns__echoFloatArrayResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoFloatArray.inputFloatArray=inputFloatArray;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoFloatArray(soap, &soap_tmp_ns__echoFloatArray);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoFloatArray(soap, &soap_tmp_ns__echoFloatArray, "ns:echoFloatArray", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoFloatArray(soap, &soap_tmp_ns__echoFloatArray, "ns:echoFloatArray", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ArrayOffloat(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoFloatArrayResponse = soap_get_ns__echoFloatArrayResponse(soap, NULL, "ns:echoFloatArrayResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoFloatArrayResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoStruct(struct soap *soap, const char *URL, const char *action, struct s__SOAPStruct _inputStruct, struct ns__echoStructResponse &result)
{
	struct ns__echoStruct soap_tmp_ns__echoStruct;
	soap->encodingStyle = "";
	soap_tmp_ns__echoStruct._inputStruct=_inputStruct;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoStruct(soap, &soap_tmp_ns__echoStruct);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoStruct(soap, &soap_tmp_ns__echoStruct, "ns:echoStruct", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoStruct(soap, &soap_tmp_ns__echoStruct, "ns:echoStruct", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ns__echoStructResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_ns__echoStructResponse(soap, &result, "ns:echoStructResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoStructArray(struct soap *soap, const char *URL, const char *action, struct ArrayOfSOAPStruct inputStructArray, struct ArrayOfSOAPStruct &_return)
{
	struct ns__echoStructArray soap_tmp_ns__echoStructArray;
	struct ns__echoStructArrayResponse *soap_tmp_ns__echoStructArrayResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoStructArray.inputStructArray=inputStructArray;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoStructArray(soap, &soap_tmp_ns__echoStructArray);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoStructArray(soap, &soap_tmp_ns__echoStructArray, "ns:echoStructArray", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoStructArray(soap, &soap_tmp_ns__echoStructArray, "ns:echoStructArray", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ArrayOfSOAPStruct(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoStructArrayResponse = soap_get_ns__echoStructArrayResponse(soap, NULL, "ns:echoStructArrayResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoStructArrayResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoVoid(struct soap *soap, const char *URL, const char *action, struct ns__echoVoidResponse &result)
{
	struct ns__echoVoid soap_tmp_ns__echoVoid;
	soap->encodingStyle = "";
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoVoid(soap, &soap_tmp_ns__echoVoid);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoVoid(soap, &soap_tmp_ns__echoVoid, "ns:echoVoid", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoVoid(soap, &soap_tmp_ns__echoVoid, "ns:echoVoid", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ns__echoVoidResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_ns__echoVoidResponse(soap, &result, "ns:echoVoidResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoBase64(struct soap *soap, const char *URL, const char *action, struct xsd__base64Binary inputBase64, struct xsd__base64Binary &_return)
{
	struct ns__echoBase64 soap_tmp_ns__echoBase64;
	struct ns__echoBase64Response *soap_tmp_ns__echoBase64Response;
	soap->encodingStyle = "";
	soap_tmp_ns__echoBase64.inputBase64=inputBase64;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoBase64(soap, &soap_tmp_ns__echoBase64);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoBase64(soap, &soap_tmp_ns__echoBase64, "ns:echoBase64", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoBase64(soap, &soap_tmp_ns__echoBase64, "ns:echoBase64", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_xsd__base64Binary(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoBase64Response = soap_get_ns__echoBase64Response(soap, NULL, "ns:echoBase64Response", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoBase64Response->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoDate(struct soap *soap, const char *URL, const char *action, char *inputDate, char *&_return)
{
	struct ns__echoDate soap_tmp_ns__echoDate;
	struct ns__echoDateResponse *soap_tmp_ns__echoDateResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoDate.inputDate=inputDate;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoDate(soap, &soap_tmp_ns__echoDate);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoDate(soap, &soap_tmp_ns__echoDate, "ns:echoDate", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoDate(soap, &soap_tmp_ns__echoDate, "ns:echoDate", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	_return = NULL;
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoDateResponse = soap_get_ns__echoDateResponse(soap, NULL, "ns:echoDateResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoDateResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoHexBinary(struct soap *soap, const char *URL, const char *action, struct xsd__hexBinary inputHexBinary, struct xsd__hexBinary &_return)
{
	struct ns__echoHexBinary soap_tmp_ns__echoHexBinary;
	struct ns__echoHexBinaryResponse *soap_tmp_ns__echoHexBinaryResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoHexBinary.inputHexBinary=inputHexBinary;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoHexBinary(soap, &soap_tmp_ns__echoHexBinary);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoHexBinary(soap, &soap_tmp_ns__echoHexBinary, "ns:echoHexBinary", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoHexBinary(soap, &soap_tmp_ns__echoHexBinary, "ns:echoHexBinary", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_xsd__hexBinary(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoHexBinaryResponse = soap_get_ns__echoHexBinaryResponse(soap, NULL, "ns:echoHexBinaryResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoHexBinaryResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoDecimal(struct soap *soap, const char *URL, const char *action, char *inputDecimal, char *&_return)
{
	struct ns__echoDecimal soap_tmp_ns__echoDecimal;
	struct ns__echoDecimalResponse *soap_tmp_ns__echoDecimalResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoDecimal.inputDecimal=inputDecimal;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoDecimal(soap, &soap_tmp_ns__echoDecimal);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoDecimal(soap, &soap_tmp_ns__echoDecimal, "ns:echoDecimal", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoDecimal(soap, &soap_tmp_ns__echoDecimal, "ns:echoDecimal", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	_return = NULL;
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoDecimalResponse = soap_get_ns__echoDecimalResponse(soap, NULL, "ns:echoDecimalResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoDecimalResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoBoolean(struct soap *soap, const char *URL, const char *action, bool inputBoolean, bool &_return)
{
	struct ns__echoBoolean soap_tmp_ns__echoBoolean;
	struct ns__echoBooleanResponse *soap_tmp_ns__echoBooleanResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echoBoolean.inputBoolean=inputBoolean;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoBoolean(soap, &soap_tmp_ns__echoBoolean);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoBoolean(soap, &soap_tmp_ns__echoBoolean, "ns:echoBoolean", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoBoolean(soap, &soap_tmp_ns__echoBoolean, "ns:echoBoolean", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_xsd__boolean(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echoBooleanResponse = soap_get_ns__echoBooleanResponse(soap, NULL, "ns:echoBooleanResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echoBooleanResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoStructAsSimpleTypes(struct soap *soap, const char *URL, const char *action, struct s__SOAPStruct _inputStruct, struct ns__echoStructAsSimpleTypesResponse &result)
{
	struct ns__echoStructAsSimpleTypes soap_tmp_ns__echoStructAsSimpleTypes;
	soap->encodingStyle = "";
	soap_tmp_ns__echoStructAsSimpleTypes._inputStruct=_inputStruct;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoStructAsSimpleTypes(soap, &soap_tmp_ns__echoStructAsSimpleTypes);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoStructAsSimpleTypes(soap, &soap_tmp_ns__echoStructAsSimpleTypes, "ns:echoStructAsSimpleTypes", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoStructAsSimpleTypes(soap, &soap_tmp_ns__echoStructAsSimpleTypes, "ns:echoStructAsSimpleTypes", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ns__echoStructAsSimpleTypesResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_ns__echoStructAsSimpleTypesResponse(soap, &result, "ns:echoStructAsSimpleTypesResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoSimpleTypesAsStruct(struct soap *soap, const char *URL, const char *action, char *inputString, long *inputInteger, float *inputFloat, struct ns__echoSimpleTypesAsStructResponse &result)
{
	struct ns__echoSimpleTypesAsStruct soap_tmp_ns__echoSimpleTypesAsStruct;
	soap->encodingStyle = "";
	soap_tmp_ns__echoSimpleTypesAsStruct.inputString=inputString;
	soap_tmp_ns__echoSimpleTypesAsStruct.inputInteger=inputInteger;
	soap_tmp_ns__echoSimpleTypesAsStruct.inputFloat=inputFloat;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoSimpleTypesAsStruct(soap, &soap_tmp_ns__echoSimpleTypesAsStruct);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoSimpleTypesAsStruct(soap, &soap_tmp_ns__echoSimpleTypesAsStruct, "ns:echoSimpleTypesAsStruct", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoSimpleTypesAsStruct(soap, &soap_tmp_ns__echoSimpleTypesAsStruct, "ns:echoSimpleTypesAsStruct", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ns__echoSimpleTypesAsStructResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_ns__echoSimpleTypesAsStructResponse(soap, &result, "ns:echoSimpleTypesAsStructResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echo2DStringArray(struct soap *soap, const char *URL, const char *action, struct ArrayOfstring2D _input2DStringArray, struct ArrayOfstring2D &_return)
{
	struct ns__echo2DStringArray soap_tmp_ns__echo2DStringArray;
	struct ns__echo2DStringArrayResponse *soap_tmp_ns__echo2DStringArrayResponse;
	soap->encodingStyle = "";
	soap_tmp_ns__echo2DStringArray._input2DStringArray=_input2DStringArray;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echo2DStringArray(soap, &soap_tmp_ns__echo2DStringArray);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echo2DStringArray(soap, &soap_tmp_ns__echo2DStringArray, "ns:echo2DStringArray", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echo2DStringArray(soap, &soap_tmp_ns__echo2DStringArray, "ns:echo2DStringArray", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ArrayOfstring2D(soap, &_return);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_tmp_ns__echo2DStringArrayResponse = soap_get_ns__echo2DStringArrayResponse(soap, NULL, "ns:echo2DStringArrayResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	_return = soap_tmp_ns__echo2DStringArrayResponse->_return;
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoNestedStruct(struct soap *soap, const char *URL, const char *action, struct s__SOAPStructStruct _inputStruct, struct ns__echoNestedStructResponse &result)
{
	struct ns__echoNestedStruct soap_tmp_ns__echoNestedStruct;
	soap->encodingStyle = "";
	soap_tmp_ns__echoNestedStruct._inputStruct=_inputStruct;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoNestedStruct(soap, &soap_tmp_ns__echoNestedStruct);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoNestedStruct(soap, &soap_tmp_ns__echoNestedStruct, "ns:echoNestedStruct", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoNestedStruct(soap, &soap_tmp_ns__echoNestedStruct, "ns:echoNestedStruct", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ns__echoNestedStructResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_ns__echoNestedStructResponse(soap, &result, "ns:echoNestedStructResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_ns__echoNestedArray(struct soap *soap, const char *URL, const char *action, struct s__SOAPArrayStruct _inputStruct, struct ns__echoNestedArrayResponse &result)
{
	struct ns__echoNestedArray soap_tmp_ns__echoNestedArray;
	soap->encodingStyle = "";
	soap_tmp_ns__echoNestedArray._inputStruct=_inputStruct;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_ns__echoNestedArray(soap, &soap_tmp_ns__echoNestedArray);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_ns__echoNestedArray(soap, &soap_tmp_ns__echoNestedArray, "ns:echoNestedArray", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_ns__echoNestedArray(soap, &soap_tmp_ns__echoNestedArray, "ns:echoNestedArray", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_ns__echoNestedArrayResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_ns__echoNestedArrayResponse(soap, &result, "ns:echoNestedArrayResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_FMAC5 int SOAP_FMAC6 soap_call_m__echoVoid(struct soap *soap, const char *URL, const char *action, struct m__echoVoidResponse &result)
{
	struct m__echoVoid soap_tmp_m__echoVoid;
	soap->encodingStyle = "";
	if (!URL)
		URL = "http://www.cs.fsu.edu/~engelen/interop2C.cgi";
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize_m__echoVoid(soap, &soap_tmp_m__echoVoid);
	soap_begin_count(soap);
	if (soap->mode & SOAP_IO_LENGTH)
	{	soap_envelope_begin_out(soap);
		soap_putheader(soap);
		soap_body_begin_out(soap);
		soap_put_m__echoVoid(soap, &soap_tmp_m__echoVoid, "m:echoVoid", "");
		soap_body_end_out(soap);
		soap_envelope_end_out(soap);
	}
	if (soap_connect(soap, URL, action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put_m__echoVoid(soap, &soap_tmp_m__echoVoid, "m:echoVoid", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	soap_default_m__echoVoidResponse(soap, &result);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get_m__echoVoidResponse(soap, &result, "m:echoVoidResponse", "");
	if (soap->error)
	{	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
			return soap_recv_fault(soap);
		return soap_closesock(soap);
	}
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
#ifndef WITH_LEANER
	 || soap_resolve_attachments(soap)
#endif
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

SOAP_END_NAMESPACE(soap)

/* end of soapClient.cpp */
