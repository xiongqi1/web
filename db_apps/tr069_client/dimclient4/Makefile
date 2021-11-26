###########################################################################
#   Copyright (C) 2004-2010 by Dimark Software Inc.                       #
#   support@dimark.com                                                    #
###########################################################################

#
# Expects the following external variables to be set
#
# PLATFORM       = Platform name
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of staging tree for install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_ALIBS     = CDCS static library path
# CDCS_SOLIBS    = CDCS dynamic library path
# MODE           = Build mode (for bovine: recovery, main)
# SBASE          = Source tree base directory

# Set a few defaults (build for Platypus)
ifeq ($(PLATFORM),Platypus)
CROSS_COMPILE ?= mipsel-linux-
INSTALLDIR    ?= $(shell readlink -f ../../../../romfs)
CDCS_INCLUDE  ?= $(shell readlink -f ../../../../staging_l/include)
CDCS_ALIBS    ?= $(shell readlink -f ../../../../staging_l/lib/)
CDCS_SOLIBS   ?= $(shell readlink -f ../../../../romfs/lib)
KERNELINC     ?= $(shell readlink -f ../../../../linux-2.6.21.x/include)
VARIANT           ?= $(shell cat "../../../../variant.last_build" 2> /dev/null )
else ifeq ($(PLATFORM),Arachnid)
CROSS_COMPILE ?= ubicom32-linux-uclibc-
INSTALLDIR    ?= $(shell readlink -f ../../../staging)
CDCS_INCLUDE  ?= $(shell readlink -f ../../../cdcs_include)
CDCS_ALIBS    ?= $(shell readlink -f ../../../cdcs_alibs)
CDCS_SOLIBS   ?= $(SBASE)/staging_l/lib
else ifeq ($(PLATFORM),Bovine)
CROSS_COMPILE ?= arm-linux-
INSTALLDIR    ?= $(shell readlink -f ../../../staging)
CDCS_INCLUDE  ?= $(shell readlink -f ../../../cdcs_include)
CDCS_ALIBS    ?= $(shell readlink -f ../../../cdcs_alibs)
CDCS_SOLIBS   ?= $(INSTALLDIR)/usr/lib
else ifeq ($(PLATFORM),X86)
CROSS_COMPILE ?=
INSTALLDIR    ?= 
CDCS_INCLUDE  ?= 
CDCS_ALIBS    ?= $(INSTALLDIR)/lib
CDCS_SOLIBS   ?= $(INSTALLDIR)/lib
else
$(error "Unknown Platform $(PLATFORM)")
endif

# The C compiler and linker
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc
STRIP=$(CROSS_COMPILE)strip

INSTALL_BIN=$(INSTALLDIR)/usr/bin
ifeq ($(PLATFORM),Platypus)
INSTALL_CONF=$(INSTALLDIR)/etc_ro
else
INSTALL_CONF=$(INSTALLDIR)/etc
endif
INSTALL_VAR=$(INSTALLDIR)/usr/local/var/tr-069

GSOAP=gsoap/soapcpp2
SOAPH=gsoap/stdsoap2.h gsoap/soapdefs.h
SOAPC=gsoap/stdsoap2.c
SOAPSRC=gsoap/soapcpp2_src
PLUGINDIR=plugin
GSOAP_SRC=gsoap/soapdefs.h

SRCDIR=src
HOSTDIR=$(SRCDIR)/host
TR104DIR=$(SRCDIR)/tr104
TR111DIR=$(SRCDIR)/tr111
OPTIONDIR=$(SRCDIR)/options
FTPDIR=$(SRCDIR)/ftp
HANDLERDIR=$(SRCDIR)/handlers
PROFILESDIR=$(SRCDIR)/profiles

ifeq ($(PLATFORM),Platypus)
SSLDIR=$(shell readlink -f ../../../openssl-0.9.8e)
SSLINC=$(SSLDIR)/include
else ifeq ($(PLATFORM),Arachnid)
SSLDIR=$(SBASE)/staging_l/lib
SSLINC=$(SBASE)/staging_l/include
endif

NSMAP=$(SRCDIR)/dim.nsmap \
	$(SRCDIR)/soapServer.c \
	$(SRCDIR)/soapClient.c \
	$(SRCDIR)/soapC.c

FTPSRC=	$(FTPDIR)/ftp.c \
	$(FTPDIR)/ftp_ft.c \
	$(FTPDIR)/cmds.c
FTPOBJ=	$(FTPDIR)/ftp.o \
	$(FTPDIR)/ftp_ft.o \
	$(FTPDIR)/cmds.o
FTPHDR= $(FTPDIR)/ftp_var.h \
	$(FTPDIR)/ftp_ft.h \
	$(FTPDIR)/cmds.h 

TR111SRC = $(TR111DIR)/stun_dimark.c \
	   $(TR111DIR)/stun_client.c \
	   $(TR111DIR)/stun_packet.c \
	   $(TR111DIR)/stun_util.c
TR111OBJ = $(TR111DIR)/stun_dimark.o \
	   $(TR111DIR)/stun_client.o \
	   $(TR111DIR)/stun_packet.o \
	   $(TR111DIR)/stun_util.o
TR111HDR = $(TR111DIR)/stun_dimark.h \
	   $(TR111DIR)/stun_client.h \
	   $(TR111DIR)/stun_packet.h \
	   $(TR111DIR)/stun_util.h

HANDLERSRC = \
	 $(HANDLERDIR)/hosthandler.c \
	 $(HANDLERDIR)/kickedhandler.c \
	 $(HANDLERDIR)/transfershandler.c \
	 $(HANDLERDIR)/acshandler.c \
 	 $(HANDLERDIR)/timehandler.c \
 	 $(HANDLERDIR)/stunhandler.c \
 	 $(HANDLERDIR)/notificationhandler.c
HANDLEROBJ = \
	 $(HANDLERDIR)/hosthandler.o \
	 $(HANDLERDIR)/kickedhandler.o \
	 $(HANDLERDIR)/transfershandler.o \
	 $(HANDLERDIR)/acshandler.o \
 	 $(HANDLERDIR)/timehandler.o \
 	 $(HANDLERDIR)/stunhandler.o \
 	 $(HANDLERDIR)/notificationhandler.o
HANDLERHDR = \
	 $(HANDLERDIR)/hosthandler.h \
	 $(HANDLERDIR)/kickedhandler.h \
	 $(HANDLERDIR)/transfershandler.h \
	 $(HANDLERDIR)/acshandler.h \
 	 $(HANDLERDIR)/timehandler.h \
 	 $(HANDLERDIR)/stunhandler.h \
 	 $(HANDLERDIR)/notificationhandler.h
 
MAINFILE=$(SRCDIR)/dimclient.c 
SRCFILES= \
     $(SRCDIR)/methods.c \
     $(SRCDIR)/serverdata.c \
	 $(SRCDIR)/filetransfer.c \
	 $(SRCDIR)/eventcode.c \
	 $(SRCDIR)/parameter.c \
	 $(SRCDIR)/callback.c \
	 $(SRCDIR)/paramaccess.c \
	 $(SRCDIR)/debug.c \
	 $(SRCDIR)/utils.c \
	 $(SRCDIR)/list.c \
	 $(SRCDIR)/vouchers.c \
	 $(SRCDIR)/paramconvenient.c \
	 $(OPTIONDIR)/option.c \
	 $(HOSTDIR)/ethParameter.c \
	 $(HOSTDIR)/diagParameter.c \
	 $(HOSTDIR)/eventStore.c \
	 $(HOSTDIR)/optionStore.c \
	 $(HOSTDIR)/parameterStore.c \
	 $(HOSTDIR)/filetransferStore.c \
	 $(HOSTDIR)/luaCore.c \
	 $(HOSTDIR)/luaDimclient.c \
	 $(HOSTDIR)/luaParameter.c \
	 $(HOSTDIR)/luaEvent.c \
	 $(HOSTDIR)/luaTransfer.c \
	 $(PROFILESDIR)/ipping_profile.c \
	 $(PROFILESDIR)/traceroute_profile.c \
	 $(PROFILESDIR)/time_profile.c \
	 $(TR104DIR)/voipParameter.c \
	 $(SRCDIR)/ftcallback.c \
	 $(TR111SRC) \
	 $(FTPSRC) \
	 $(HANDLERSRC)

GSOAP_PLUGIN_SRC=$(PLUGINDIR)/httpda.c \
         $(PLUGINDIR)/smdevp.c \
         $(PLUGINDIR)/md5evp.c \

GSOAPSRC=$(SRCDIR)/soapC.c \
	 $(SRCDIR)/soapClient.c \
	 $(SRCDIR)/soapServer.c

OBJ_FILES= \
	$(SRCDIR)/dimclient.o \
	$(SRCDIR)/methods.o \
	$(SRCDIR)/serverdata.o \
	$(SRCDIR)/filetransfer.o \
	$(SRCDIR)/eventcode.o \
	$(SRCDIR)/parameter.o \
	$(SRCDIR)/callback.o \
	$(SRCDIR)/paramaccess.o \
	$(SRCDIR)/debug.o \
	$(SRCDIR)/utils.o \
	$(SRCDIR)/list.o \
	$(SRCDIR)/vouchers.o \
	$(SRCDIR)/paramconvenient.o \
	$(HOSTDIR)/ethParameter.o \
	$(HOSTDIR)/diagParameter.o \
	$(HOSTDIR)/eventStore.o \
	$(HOSTDIR)/optionStore.o \
	$(HOSTDIR)/parameterStore.o \
	$(HOSTDIR)/filetransferStore.o \
	$(HOSTDIR)/luaCore.o \
	$(HOSTDIR)/luaDimclient.o \
	$(HOSTDIR)/luaParameter.o \
	$(HOSTDIR)/luaEvent.o \
	$(HOSTDIR)/luaTransfer.o \
	$(PROFILESDIR)/ipping_profile.o \
	$(PROFILESDIR)/traceroute_profile.o \
	$(PROFILESDIR)/time_profile.o \
	$(TR104DIR)/voipParameter.o \
	$(OPTIONDIR)/option.o \
	$(SRCDIR)/soapC.o \
	$(SRCDIR)/soapClient.o \
	$(SRCDIR)/soapServer.o \
	$(PLUGINDIR)/httpda.o \
	$(PLUGINDIR)/smdevp.o \
	$(PLUGINDIR)/md5evp.o \
	$(SRCDIR)/ftcallback.o \
	gsoap/stdsoap2.o \
    $(TR111OBJ) \
	$(FTPOBJ) \
	$(HANDLEROBJ)

MAINHEADER=$(SRCDIR)/methods.h
SRCHEADER= \
	 $(SRCDIR)/dimclient.h \
	 $(SRCDIR)/globals.h \
	 $(SRCDIR)/server.h \
	 $(SRCDIR)/filetransfer.h \
	 $(SRCDIR)/eventcode.h \
	 $(SRCDIR)/parameter.h \
	 $(SRCDIR)/callback.h \
	 $(SRCDIR)/paramaccess.h \
	 $(SRCDIR)/debug.h \
	 $(SRCDIR)/utils.h \
	 $(SRCDIR)/list.h \
	 $(SRCDIR)/vouchers.h \
	 $(SRCDIR)/paramconvenient.h \
	 $(HOSTDIR)/ethParameter.h \
	 $(HOSTDIR)/diagParameter.h \
	 $(HOSTDIR)/eventStore.h \
	 $(HOSTDIR)/optionStore.h \
	 $(HOSTDIR)/parameterStore.h \
	 $(HOSTDIR)/filetransferStore.h \
	 $(HOSTDIR)/storage.h \
	 $(PROFILESDIR)/ipping_profile.h \
	 $(PROFILESDIR)/traceroute_profile.h \
	 $(PROFILESDIR)/time_profile.h \
	 $(TR104DIR)/voipParameter.h \
	 $(OPTIONDIR)/option.h \
	 $(SRCDIR)/ftcallback.h \
	 $(TR111HDR) \
	 $(FTPHDR) \
	 $(LIBXMLHDR) \
	 $(HANDLERHDR)

GSOAP_PLUGIN_HEADER=\
	 $(PLUGINDIR)/httpda.h \
	 $(PLUGINDIR)/smdevp.h \
	 $(PLUGINDIR)/threads.h \
	 $(PLUGINDIR)/md5evp.h

ifeq ($(PLATFORM),Platypus)
DIM_LIBS=-lpthread -L$(SSLDIR) -lcrypto -lssl
else
DIM_LIBS=-lpthread -lcrypto -lssl
endif
CONV_LIBS= -lxml2 -I/usr/include/libxml2

#COFLAGS=-O3
#CWFLAGS=-Wall #-Werror

ifeq ($(PLATFORM),Platypus)
# WITHOUT_DEBUG=TRUE
else
#WITHOUT_DEBUG=TRUE
endif

ifndef WITHOUT_DEBUG
ifeq ($(PLATFORM),Platypus)
CDFLAGS=-DDEBUG -D_DEBUG -Wall -g
else
CDFLAGS=-DDEBUG -D_DEBUG -Wall -g #-DSOAP_DEBUG 
endif
#-DWITH_LEAN #-DWITH_LEANER
endif
CF = $(COFLAGS) $(CWFLAGS) $(CDFLAGS)

ifeq ($(PLATFORM),Avian)
   CF += -D PLATFORM_AVIAN
else ifeq ($(PLATFORM),Bovine)
   CF += -D PLATFORM_BOVINE -D CONNECTION_REQUEST_HACK
else ifeq ($(PLATFORM),Platypus)
   CF += -D PLATFORM_PLATYPUS -D CONNECTION_REQUEST_HACK
else ifeq ($(PLATFORM),Arachnid)
   CF += -D PLATFORM_ARACHNID -D CONNECTION_REQUEST_HACK $(PLATFORM_CFLAGS)
else ifeq ($(PLATFORM),X86)
   CF += -D PLATFORM_X86
else ifeq ($(MAKECMDGOALS),clean)
   # We let this pass
else ifeq ($(MAKECMDGOALS),distclean)
   # We let this pass
else
   $(error "Unknown Platform $(PLATFORM)")
endif

ifeq ($(V_BOARD),3g36wv)
	CF += -D CONNECTION_REQUEST_RESET_NONCE
endif

CF +=-DBOARD_$(V_BOARD)=y -DSKIN_$(V_SKIN)=y 

CTRFLAGS=-DTR_111_GATEWAY
#CTRFLAGS=-DTR_111_DEVICE -DTR_111_DEVICE_WITHOUT_GATEWAY
#CCOMFLAG=-DWITH_STUN_CLIENT
#CPRODFLAG=-DACS_REGMAN

CSSLFLAG=-DWITH_OPENSSL #-DWITH_SSLAUTH 
CMFLAGS=-DWITH_COOKIES 
CSYSFLAG=-DWITH_SOAPDEFS_H

ifeq ($(PLATFORM),Platypus)
CINCFLAG=-Igsoap -I$(OPTIONDIR) -I$(SRCDIR) -I$(HOSTDIR) -I$(TR104DIR) -I$(TR111DIR) -I$(PLUGINDIR) -I$(FTPDIR) -I$(HANDLERDIR) -I$(SSLINC)
else ifeq ($(PLATFORM),Arachnid)
CINCFLAG=-Igsoap -I$(OPTIONDIR) -I$(SRCDIR) -I$(HOSTDIR) -I$(TR104DIR) -I$(TR111DIR) -I$(PLUGINDIR) -I$(FTPDIR) -I$(HANDLERDIR) -I$(SSLINC)
else
CINCFLAG=-Igsoap -I$(OPTIONDIR) -I$(SRCDIR) -I$(HOSTDIR) -I$(TR104DIR) -I$(TR111DIR) -I$(PLUGINDIR) -I$(FTPDIR) -I$(HANDLERDIR)
endif
CIFLAGS=$(CTRFLAGS) $(CSSLFLAG) $(CPRODFLAG) $(CCOMFLAG) $(CINCFLAG)
CFLAGS= $(CWFLAGS) $(CDFLAGS) $(CIFLAGS) $(CMFLAGS) $(CSYSFLAG)
DEB_FLAGS= $(CWFLAGS) $(CDFLAGS) $(CIFLAGS) $(CMFLAGS) $(CSYSFLAG)

ifeq ($(PLATFORM),Platypus)
LUA_HOME=../../gnu_apps/lua/src
else
LUA_HOME=../../../gnu_apps/lua/src
endif

LUA_INC=-I$(LUA_HOME)/src
ifeq ($(PLATFORM),Platypus)
LUA_LIB=-lm -L$(CDCS_SOLIBS) -llua -rdynamic
else
LUA_LIB=-lm -llua -L$(CDCS_SOLIBS) -rdynamic
endif
LUA_STUFF=$(LUA_INC) $(LUA_LIB)

all:		dimclient


distclean:	clean


clean:
		rm -rf dimclient host-if conv-util \
		gsoap/*.o $(PLUGINDIR)/*.o src/*.o $(HOSTDIR)/*.o $(TR104DIR)/*.o $(OPTIONDIR)/*.o $(FTPDIR)/*.o $(TR111DIR)/*.o $(HANDLERDIR)/*.o $(PROFILESDIR)/*.o \
		src/soap*.* src/dim*.xml src/dim.nsmap src/dim.wsdl src/cwmp.xsd *.log
		
release:
		make clean
		rm -rf .cproject .project .svn DOC/.svn README/.svn TR-098/.svn TR-106/.svn \
		gsoap/.svn $(PLUGINDIR)/.svn src/.svn $(HOSTDIR)/.svn $(TR104DIR)/.svn $(OPTIONDIR)/.svn $(FTPDIR)/.svn $(TR111DIR)/.svn $(HANDLERDIR)/.svn $(PROFILESDIR)/.svn

install:
		make all
		mkdir -p $(INSTALL_BIN)
		cp dimclient $(INSTALL_BIN)/dimclient
		$(STRIP) $(INSTALL_BIN)/dimclient
		mkdir -p $(INSTALL_CONF)
		cp log.config $(INSTALL_CONF)/tr-069.log.conf

$(NSMAP):	$(MAINHEADER)	
		$(GSOAP) -w -x -L -c -d $(SRCDIR) $(MAINHEADER)

%.o:%.c		$(NSMAP) $(MAINHEADER)
		$(CC) -c $(CF) $(DEB_FLAGS) $(LUA_INC) $< -o $@
		@touch $@

dimclient:	$(OBJ_FILES)
		$(LD) $(CF) $(OBJ_FILES) -o $@ $(DIM_LIBS) $(LUA_STUFF)
#		$(CC) $(CF) conv-util.c -o conv-util $(CONV_LIBS)
#		$(CC) $(CF) host-if.c -o host-if
		@touch $@
		
