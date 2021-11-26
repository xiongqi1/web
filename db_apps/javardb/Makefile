#
# Expects the following external variables to be set
#
# PATH           = Must include a path to the correct toolchain
# CROSS_COMPILE  = Cross-compiler prefix, e.g. arm-linux-
# INSTALLDIR     = Root of real staging tree for install
# DEFPREFIX      = Path to install to on target
# TEMPINSTALL    = Path to local temp install dir
# SCAT_ARGS      = Options for scatter_install
# CDCS_INCLUDE   = CDCS include files, including RDB, libs, etc.
# CDCS_LIB       = CDCS library path
# V_JAVA	 = JRE name


# Might override this in the caller later.
TEMPINSTALL ?= tempinstall

JAVAHOME := $(TEMPINSTALL)/$(DEFPREFIX)java
BINDIR := $(TEMPINSTALL)/$(DEFPREFIX)bin

LIB_JNIC := $(JAVAHOME)/lib/arm/libjavardb.so
LIB_JAVA := javardb.jar
SCAT_BASE := scatter_install.sh -b -D $(DEFPREFIX)java $(TEMPINSTALL) $(INSTALLDIR)

# set compiler environment variables
CC := $(CROSS_COMPILE)gcc

CFLAGS:=-Wall -shared -Os -fPIC
LDFLAGS:=-Wall -pedantic -fPIC

.PHONY: all tempsetup
all: tempsetup $(LIB_JNIC) $(LIB_JAVA) $(V_JAVA).tar.gz
	tar -xzC $(JAVAHOME) -f $(V_JAVA).tar.gz
	cp $(LIB_JAVA) $(JAVAHOME)/lib/ext
	(cd $(BINDIR)/ && cp -s ../java/bin/* .)

tempsetup:
	rm -Rf $(TEMPINSTALL)/$(DEFPREFIX)
	mkdir -p $(JAVAHOME)/lib/arm/
	mkdir -p $(BINDIR)

# Using -l:librdb.so.1 rather than -lrdb avoids the need for a linker-name symlink
# There's no soname in this because the only thing that uses it is the .jar
# and the System.loadLibrary() in RDB.java uses the base name anyway
$(LIB_JNIC): tempsetup
	$(CC) $(CFLAGS) -I jdk_headers -I$(CDCS_INCLUDE) -Ijni -l:librdb.so.1 -lm -L $(CDCS_LIB) -o $(LIB_JNIC) jni/*.c

# .PHONY: install
#install: $(LIB_JNIC)
#	mkdir -p $(PACKAGE)/usr/local/
#	cp -av $(V_JAVA)/* $(PACKAGE)/usr/local
#	$(STRIP) $(LIB_JNIC)
#	cp -v $(LIB_JNIC) $(PACKAGE)/usr/local/lib/arm
#	cp -v javardb.jar $(PACKAGE)/usr/local/lib/ext
#	cp -rv CONTROL $(PACKAGE)

# Not sure if scatter_install makes sense for an IPK.
# The VM itself is already stripped anyway
#	scatter_install.sh -s $(STRIP) "$(V_JAVA)" "$(JAVAHOME)" "$(JAVAHOME)" -b -D /lib
#	scatter_install.sh -s $(STRIP) "$(LIB_JNIC)" "$(JAVAHOME)/lib" "$(JAVAHOME)/lib"
#	cp -v javardb.jar $(JAVAHOME)/lib/ext
#	cp -rv CONTROL $(PACKAGE)

.PHONY: clean distclean install package
install: all
	$(SCAT_BASE) $(SCAT_ARGS)
package: all
	$(SCAT_BASE)/javardb  $(SCAT_ARGS)

distclean clean:
	rm -Rf *.o *.so $(TEMPINSTALL)

