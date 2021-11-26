GSOAP=../../soapcpp2
GWSDL=../../wsdl2h
SOAPH=../../stdsoap2.h
SOAPC=../../stdsoap2.c
SOAPCPP=../../stdsoap2.cpp
CC=gcc
CPP=g++
LIBS=-lcrypto -lssl
COFLAGS=-O2
CWFLAGS=-Wall
CIFLAGS=-I. -I../.. -I../../plugin
CMFLAGS=-DWITH_DOM -DWITH_OPENSSL
CFLAGS= $(CWFLAGS) $(COFLAGS) $(CIFLAGS) $(CMFLAGS)
all:		wssedemo
wssedemo:	wssedemo.h wssedemo.c soapC.c $(SOAPH) $(SOAPC) ../../dom.c wsseapi.o smdevp.o
		$(CC) $(CFLAGS) -o wssedemo wssedemo.c soapC.c soapClient.c soapServer.c ../../dom.c $(SOAPC) wsseapi.o smdevp.o $(LIBS)
wssedemo.h:	wssedemo.wsdl
		$(GWSDL) -c -t ../../typemap.dat wssedemo.wsdl
soapC.c:	wssedemo.h
		$(GSOAP) -c -I../../import wssedemo.h
wsseapi.o:	../../plugin/wsseapi.h ../../plugin/wsseapi.c
		$(CC) $(CFLAGS) -c ../../plugin/wsseapi.c
smdevp.o:	../../plugin/smdevp.h ../../plugin/smdevp.c
		$(CC) $(CFLAGS) -c ../../plugin/smdevp.c
clean:
		rm -f *.o soapH.h soapStub.h soapC.cpp soapC.c soapClient.cpp soapClient.c soapServer.cpp soapServer.c soap*Proxy.h soap*Object.h soapClientLib.c soapServerLib.c
distclean:
		rm -f *.o *.xml *.nsmap *.log soapH.h soapStub.h soapC.cpp soapC.c soapClient.cpp soapClient.c soapServer.cpp soapServer.c soapClientLib.c soapServerLib.c soap*Proxy.h soap*Object.h wssedemo wssedemo.h
