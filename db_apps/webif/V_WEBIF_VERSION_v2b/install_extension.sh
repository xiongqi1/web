#!/bin/bash
#
# This script is intended to generate customised lua scripts that are used to
# override part of generic lua scripts. The customisation is based on
# V-variables.
#

usage() {
    test -n "$1" && echo "Error: $1." && exit 1
    echo "Usage: install_extension.sh <variant.sh path> <source dir> <install dir>"
    exit 2
}

INSTALL_SEQ=1
install() {
    FNAME=$1
    VVAR=$2
    VAL=$3
    VAR_FILE="$SRCDIR/$VVAR/${FNAME}_${VAL}.lua"
    test -f "$VAR_FILE" || return 0
    FNUM=$(printf "%04d" ${INSTALL_SEQ})
    echo "cp $VAR_FILE $INSTALL/${FNAME}_${FNUM}.lua"
    cp "$VAR_FILE" "$INSTALL/${FNAME}_${FNUM}.lua"
    INSTALL_SEQ=$(($INSTALL_SEQ+1))
}

test $# -ne 3 && usage

VFILE=$1
SRCDIR=$2
INSTALL=$3

PREVARS=
POSTVARS="V_SKIN V_CUSTOM_FEATURE_PACK V_PRODUCT V_TITAN_INSTALLATION_ASSISTANT"

# Specifiy the list of files that can be customised.
FILES="config"

# Sanity checks
test -f "$VFILE" || usage "No valid HBASE variable detected"

# Extract variant variables
VVARIABLES=""
while read line; do
    if echo "$line" | grep -v -q '.\+=.\+'; then
        continue;
    fi
    NAME=$(echo "$line" | cut -d = -f 1)
    VALUE=$(echo "$line" | cut -d = -f 2)
    test -d "$SRCDIR/$NAME" && eval $NAME=$VALUE
    VVARIABLES+="$NAME "
done <"$VFILE"

# Pre-Ordered variables
for VVAR in $PREVARS; do
    eval VAL=\$$VVAR
    for FILE in $FILES; do
        install "$FILE" "$VVAR" "$VAL"
    done
done

# Alphabetical-Ordered variables
for VVAR in $VVARIABLES; do
    copy_now=true
    for ORDERVAR in $PREVARS $POSTVARS; do
        if [ "$VVAR" = "$ORDERVAR" ]; then
            copy_now=false
            break
        fi
    done
    if $copy_now; then
        eval VAL=\$$VVAR
        for FILE in $FILES; do
            install "$FILE" "$VVAR" "$VAL"
        done
    fi
done

# Post-Ordered variables
for VVAR in $POSTVARS; do
    eval VAL=\$$VVAR
    for FILE in $FILES; do
        install "$FILE" "$VVAR" "$VAL"
    done
done

exit $?
