CC=gcc
CPP=g++
LIBS=
COFLAGS=-O2
CWFLAGS=-Wall
CIFLAGS=-I.. -I../..
CMFLAGS=
CFLAGS= $(CWFLAGS) $(COFLAGS) $(CIFLAGS) $(CMFLAGS)
main:		main.c envC.o stdsoap2.o quoteClientLib.o rateClientLib.o calcServerLib.o
		$(CC) $(CFLAGS) -o main main.c envC.o stdsoap2.o quoteClientLib.o rateClientLib.o calcServerLib.o
stdsoap2.o:	stdsoap2.h stdsoap2.c
		$(CC) $(CFLAGS) -DWITH_NONAMESPACES -c stdsoap2.c
envC.o:		env.h header.h fault.h
		soapcpp2 -CS -cpenv env.h
		$(CC) $(CFLAGS) -c envC.c
quoteClientLib.c:	quote.h
		soapcpp2 -C -cn -pquote quote.h
quoteClientLib.o:	quoteClientLib.c
		$(CC) $(CFLAGS) -c quoteClientLib.c
rateClientLib.c:	rate.h
		soapcpp2 -C -cn -prate rate.h
rateClientLib.o:	rateClientLib.c
		$(CC) $(CFLAGS) -c rateClientLib.c
calcServerLib.c:	calc.h
		soapcpp2 -S -cn -pcalc calc.h
calcServerLib.o:	calcServerLib.c
		$(CC) $(CFLAGS) -c calcServerLib.c
clean:		
		rm -f *.o *.nsmap *H.h *Stub.h *C.cpp *C.c *Client.cpp *Client.c *ClientLib.c *ClientLib.cpp *Server.cpp *Server.c *ServerLib.c *ServerLib.cpp *ServiceObject.h *ServiceProxy.h
distclean:		
		rm -f main *.o *.wsdl *.xsd *.xml *.nsmap *H.h *Stub.h *C.cpp *C.c *Client.cpp *Client.c *ClientLib.c *ClientLib.cpp *Server.cpp *Server.c *ServerLib.c *ServerLib.cpp *ServiceObject.h *ServiceProxy.h
