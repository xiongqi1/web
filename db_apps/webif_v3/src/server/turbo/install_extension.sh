#!/bin/bash
#
# This script is intended to generate customised lua scripts that are used to
# override part of generic lua scripts. The customisation is based on
# V-variables.
#

usage() {
    test -n "$1" && echo "Error: $1." && exit 1
    echo "Usage: install_extension.sh <variant.sh path> <variants dir> <install dir>"
    exit 2
}

test $# -ne 3 && usage

VFILE=$1
VARIANTS_DIR=$2
INSTALL=$3

# Sanity checks
test -f "$VFILE" || usage "File $VFILE does not exist"

INSTALL_SEQ=1
install() {
    FNAME=$1
    VVAR=$2
    VAL=$3
    VAR_FILE="$VARIANTS_DIR/$VVAR/${FNAME}_${VAL}.lua"
    test -f "$VAR_FILE" || return 0
    FNUM=$(printf "%04d" ${INSTALL_SEQ})
    echo "cp $VAR_FILE $INSTALL/${FNAME}_${FNUM}.lua"
    cp "$VAR_FILE" "$INSTALL/${FNAME}_${FNUM}.lua"
    INSTALL_SEQ=$(($INSTALL_SEQ+1))
}

POSTVARS="V_SKIN V_CUSTOM_FEATURE_PACK V_PRODUCT"

# Specifiy the list of files that can be customised.
FILES="config handler"

# Extract variant variables
VVARIABLES=""
while read line; do
    if echo "$line" | grep -v -q '.\+=.\+'; then
        continue;
    fi
    NAME=$(echo "$line" | cut -d = -f 1)
    VALUE=$(echo "$line" | cut -d = -f 2)
    test -d "$VARIANTS_DIR/$NAME" && eval $NAME=$VALUE
    if [ "$NAME" != "V_WEBIF_SPEC" -a "$NAME" != "V_PRODUCT" -a "$NAME" != "V_SKIN" -a "$NAME" != "V_CUSTOM_FEATURE_PACK" ]; then
        VVARIABLES+="$NAME "
    fi
done <"$VFILE"

[ -n "$V_WEBIF_SPEC" ] && VVARIABLES="V_WEBIF_SPEC $VVARIABLES"
for v_var in $POSTVARS; do
    [ -n "$v_var" ] && VVARIABLES+="$v_var "
done

# first, default files
for FILE in $FILES; do
    cp "default/${FILE}.lua" "$INSTALL/${FILE}.lua"
done

# from V_vars
for VVAR in $VVARIABLES; do
    eval VAL=\$$VVAR
    for FILE in $FILES; do
        install "$FILE" "$VVAR" "$VAL"
    done
done

exit $?
