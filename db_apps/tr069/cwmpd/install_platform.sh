#!/bin/bash

usage() {
	test -n "$1" && echo "Error: $1." && exit 1
	echo "Usage: install.sh <variant.sh path> <source dir> <install root>"
	exit 2
}

install() {
	test -d "$1" || return 0
	mkdir -p $CONF_DIR $CODE_DIR $CODE_DIR/classes $CODE_DIR/handlers $CODE_DIR/scripts
	test -f $1/tr-069.conf	&& cp $1/tr-069.conf $CONF_DIR
	test -d $1/core/	&& cp $1/core/*.lua $CODE_DIR
	test -d $1/core/classes	&& ./install_classes.sh $1/core/classes $CODE_DIR/classes
	test -d $1/scripts	&& cp $1/scripts/* $CODE_DIR/scripts
	test -d $1/handlers	&& cp $1/handlers/*.lua $CODE_DIR/handlers
}

test $# -ne 3 && usage

VFILE=$1
SRCDIR=$2
INSTALL=$3

CONF_DIR=$INSTALL/etc
CODE_DIR=$INSTALL/usr/lib/tr-069

if [ "$PLATFORM" == "Platypus" ] ; then
	CONF_DIR=$INSTALL/etc_ro
fi

# Sanity checks
test -f "$VFILE" || usage "No valid HBASE variable detected"

# Extract variant variables
VVARIABLES=""
while read line; do
	if echo "$line" | grep -v -q '.\+=.\+'; then continue; fi
	NAME=`echo "$line" | cut -d = -f 1`
	VALUE=`echo "$line" | cut -d = -f 2`
#	echo $NAME=$VALUE
	eval $NAME=$VALUE
	VVARIABLES+="$NAME "
done <"$VFILE"

# make sure there is a base install for the platform-type
test -d $SRCDIR/V_TR069/$V_TR069 || usage "No '$V_TR069' directory for V_TR069 value"

# install common first
install $SRCDIR/common

# then install V_TR069 stuff
install $SRCDIR/V_TR069/$V_TR069

# install any other V_VARIABLE override files (not ordered!)
for VVAR in $VVARIABLES ; do
	if [ $VVAR == "V_TR069" ] ; then continue ; fi
	eval VAL=\$$VVAR
#	echo $VVAR $VAL
	test -d "$SRCDIR/$VVAR/$VAL" && install "$SRCDIR/$VVAR/$VAL" || true
done

exit $?
