This directory contains common #import-ed items.

dom.h		DOM support (requires dom.c/pp)
stldeque.h	std::deque serializer
stllist.h	std::list serializer
stlset.h	std::set serializer
stlvector.h	std::vector serializer
stl.h		std::deque, std::list, std::set, and std::vector serializer
soap12.h	SOAP 1.2 namespaces

This directory further contains files to support a growing number of WS-*
specifications.

For example, wsa.h is generated from WS/WS-Addressing.xsd with:

wsdl2h -cgy -o wsa.h -t WS/WS-typemap.dat WS/WS-Addressing.xsd

The typemap.dat file that is needed by wsdl2h to convert WSDL to a gSOAP header
file defines imported namespaces as follows:

wsa = <http://schemas.xmlsoap.org/ws/2004/08/addressing>

This ensures that the WS-Addressing support is #import-ed from "wsa.h" when
wsdl2h sees WS-Addressing elements. Thus, it does not attempt to extract the
WS-Addressing schema but instead uses the pre-build wsa.h definitions.

Note that the regular XML namespace bindings are defined in typemap.dat using
quotes, for example:

aws = "urn:PI/DevCentral/SoapService"
