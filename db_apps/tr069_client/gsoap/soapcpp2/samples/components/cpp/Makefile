CC=gcc
CPP=g++
LIBS=
COFLAGS=-O2
CWFLAGS=-Wall
CIFLAGS=-I.. -I../..
CMFLAGS=
CFLAGS= $(CWFLAGS) $(COFLAGS) $(CIFLAGS) $(CMFLAGS)
main:		main.cpp envC.o stdsoap2.o quoteClientLib.o rateClientLib.o calcServerLib.o
		$(CPP) $(CFLAGS) -o main main.cpp envC.o stdsoap2.o quoteClientLib.o rateClientLib.o calcServerLib.o
stdsoap2.o:	stdsoap2.h stdsoap2.cpp
		$(CPP) $(CFLAGS) -DWITH_NONAMESPACES -c stdsoap2.cpp
envC.o:		env.h header.h fault.h
		soapcpp2 -CS -penv env.h
		$(CPP) $(CFLAGS) -c envC.cpp
quoteClientLib.cpp:	quote.h
		soapcpp2 -C -n quote.h
quoteClientLib.o:	quoteClientLib.cpp
		$(CPP) $(CFLAGS) -c quoteClientLib.cpp
rateClientLib.cpp:	rate.h
		soapcpp2 -C -n rate.h
rateClientLib.o:	rateClientLib.cpp
		$(CPP) $(CFLAGS) -c rateClientLib.cpp
calcServerLib.cpp:	calc.h
		soapcpp2 -S -n calc.h
calcServerLib.o:	calcServerLib.cpp
		$(CPP) $(CFLAGS) -c calcServerLib.cpp
clean:		
		rm -f *.o *.nsmap *H.h *Stub.h *C.cpp *C.c *Client.cpp *Client.c *ClientLib.c *ClientLib.cpp *Server.cpp *Server.c *ServerLib.c *ServerLib.cpp *ServiceObject.h *ServiceProxy.h
distclean:		
		rm -f main *.o *.wsdl *.xsd *.xml *.nsmap *H.h *Stub.h *C.cpp *C.c *Client.cpp *Client.c *ClientLib.c *ClientLib.cpp *Server.cpp *Server.c *ServerLib.c *ServerLib.cpp *ServiceObject.h *ServiceProxy.h
