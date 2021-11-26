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
# For the consistency with fsextra and defconfig,
# V_SKIN, V_CUSTOM_FEATURE_PACK, V_PRODUCT are appended on the last.
#
# The order is as below(bottom has more priority)
# - V_TR069
# - Other V variables with same order in variant.sh
# - V_SKIN
# - V_CUSTOM_FEATURE_PACK
# - V_PRODUCT
VVARIABLES=""
while read line; do
	if echo "$line" | grep -v -q '.\+=.\+'; then continue; fi
	NAME=`echo "$line" | cut -d = -f 1`
#   VALUE should be everything to the right of the first equal sign even
#   if another equal sign exists in the variable's value
    VALUE=${line#*=}
	eval $NAME=$VALUE
	if [ "$NAME" != "V_PRODUCT" -a "$NAME" != "V_SKIN" -a "$NAME" != "V_CUSTOM_FEATURE_PACK" -a "$NAME" != "V_TR069" ]; then
		VVARIABLES+="$NAME "
	fi
done <"$VFILE"

if [ -n "$V_SKIN" ]; then
	VVARIABLES+="V_SKIN "
fi
if [ -n "$V_CUSTOM_FEATURE_PACK" ]; then
	VVARIABLES+="V_CUSTOM_FEATURE_PACK "
fi
VVARIABLES+="V_PRODUCT "

# make sure there is a base install for the platform-type
test -d $SRCDIR/V_TR069/$V_TR069 || usage "No '$V_TR069' directory for V_TR069 value"

# install common first
install $SRCDIR/common

CONF_SUFFIX=".conf"
for VVAR in $VVARIABLES ; do
	if [ $VVAR == "V_TR181" ] ; then
		eval VAL=\$$VVAR
#		echo $VVAR $VAL
		if [ "$VAL" = "1" -o "$VAL" = "2" ] ; then
			CONF_SUFFIX="_v2.conf"
		fi
		break
	fi
done
BASE_CONF="tr-069_base$CONF_SUFFIX"

cp -f "$SRCDIR/V_TR069/$V_TR069/$BASE_CONF" "$SRCDIR/V_TR069/$V_TR069/tr-069.conf"

# then install V_TR069 stuff
install $SRCDIR/V_TR069/$V_TR069

# install any other V_VARIABLE override files (ordered as above)
# we allow two sets of add/del_tr069 overwrites based on conf file suffix:
# .conf vs _v2.conf, so that both TR-098 and TR-181 are supported.
# note: the override file tr-069.conf should never have a _v2 suffix.
for VVAR in $VVARIABLES ; do
	eval VAL=\$$VVAR
#	echo $VVAR $VAL
	test -d "$SRCDIR/$VVAR/$V_TR069/$VAL" && install "$SRCDIR/$VVAR/$V_TR069/$VAL" || true
	if [ -f "$SRCDIR/$VVAR/$V_TR069/$VAL/conf/del/del_tr069$CONF_SUFFIX" ]; then
		./confMerger.sh -d "$CONF_DIR/tr-069.conf" "$SRCDIR/$VVAR/$V_TR069/$VAL/conf/del/del_tr069$CONF_SUFFIX"
	fi
	if [ -f "$SRCDIR/$VVAR/$V_TR069/$VAL/conf/add/add_tr069$CONF_SUFFIX" ]; then
		./confMerger.sh -a "$CONF_DIR/tr-069.conf" "$SRCDIR/$VVAR/$V_TR069/$VAL/conf/add/add_tr069$CONF_SUFFIX"
	fi
	if [ -f "$SRCDIR/$VVAR/$V_TR069/$VAL/tr-069.conf" ]; then
		# Skip the rest of V-VARs if tr-069.conf exists to prevent polluting conf.
		# Normally, only add/del_tr069.conf should exist under variants/.
		# But if we do provide a tr-069.conf, then this conf is deemed final.
		break
	fi
done

cp -f "$CONF_DIR/tr-069.conf" "$SRCDIR/V_TR069/$V_TR069/tr-069.conf"

exit $?
