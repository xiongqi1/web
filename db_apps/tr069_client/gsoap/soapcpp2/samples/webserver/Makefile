GSOAP=../../soapcpp2
SOAPH=../../stdsoap2.h
SOAPC=../../stdsoap2.c
SOAPCPP=../../stdsoap2.cpp
CC=gcc
CPP=g++
LIBS=-lpthread -lz -lssl -lcrypto
COFLAGS=-O2
CWFLAGS=-Wall
CIFLAGS=-I../.. -I../../plugin
CMFLAGS=-DWITH_COOKIES -DWITH_GZIP -DWITH_OPENSSL
CFLAGS= $(CWFLAGS) $(COFLAGS) $(CIFLAGS) $(CMFLAGS)
all:		webserver
webserver:	webserver.h webserver.c logging.o httpget.o httpform.o threads.o options.o $(SOAPH) $(SOAPC)
		$(GSOAP) -c webserver.h
		$(CC) $(CFLAGS) -o webserver webserver.c logging.o httpget.o httpform.o threads.o options.o soapC.c soapClient.c soapServer.c $(SOAPC) $(LIBS)
options.o:	opt.h options.h options.c
		$(GSOAP) -cnpopt opt.h
		$(CC) $(CFLAGS) -c options.c
logging.o:	../../plugin/logging.h ../../plugin/logging.c
		$(CC) $(CFLAGS) -c ../../plugin/logging.c
httpget.o:	../../plugin/httpget.h ../../plugin/httpget.c
		$(CC) $(CFLAGS) -c ../../plugin/httpget.c
httpform.o:	../../plugin/httpform.h ../../plugin/httpform.c
		$(CC) $(CFLAGS) -c ../../plugin/httpform.c
threads.o:	../../plugin/threads.h ../../plugin/threads.c
		$(CC) $(CFLAGS) -c ../../plugin/threads.c
clean:
		rm -f *.o soapH.h soapStub.h soapC.cpp soapC.c soapClient.cpp soapClient.c soapServer.cpp soapServer.c soap*Proxy.h
distclean:
		rm -f *.o *.wsdl *.xsd *.xml *.nsmap *.log soapH.h soapStub.h soapC.cpp soapC.c soapClient.cpp soapClient.c soapServer.cpp soapServer.c soapClientLib.c soapClientLib.cpp soapServerLib.c soapServerLib.cpp soap*Proxy.h webserver
