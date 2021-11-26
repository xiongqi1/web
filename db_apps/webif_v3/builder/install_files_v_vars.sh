#!/bin/bash
#
# Copyright (C) Casa Systems.
#
# Installing files, overridden by V_VARs, from source to destination directory
#
# inspired by fsextra install script

nof=${0##*/}

cd $(dirname $0)
RDIR=$(/bin/pwd)


usage() {
    if [ -n "$1" ]; then
        echo 1>&2
        echo "$@" 1>&2
    fi
    cat 1>&2 <<EOM

Usage: ${nof} <path to variant.sh> <source dir> <target dir>

EOM
    exit 1
}

if [ $# -lt 3 ]; then
    usage "Insufficient parameters"
fi

VFILE=$1
SRCDIR=$2
TGTDIR=$3

# We track errors and then fail at the end
RETVAL=0
FLIST=flist; #Temp file

# Sanity checks
if [ ! -f "$VFILE" ]; then
    usage "$VFILE does not exist"
fi
if [ ! -d "$SRCDIR" ]; then
    usage "$SRCDIR does not exist"
fi
if [ ! -d "$TGTDIR" ]; then
    usage "$TGTDIR does not exist"
fi

# Extract variant variables. We do this instead of sourcing because
# this way we also extract a list of variables.
VVARIABLES=''
while read line; do
    if echo "$line" | grep -v -q '.\+=.\+'; then continue; fi
    NAME=$(echo "$line" | cut -d = -f 1)
    # VALUE should be everything to the right of the first equal sign even
    # if another equal sign exists in the variable's value
    VALUE=${line#*=}
    eval $NAME=$VALUE
    if [ "$NAME" != "V_WEBIF_SPEC" -a "$NAME" != "V_PRODUCT" -a "$NAME" != "V_SKIN" -a "$NAME" != "V_CUSTOM_FEATURE_PACK" ]; then
        VVARIABLES+="$NAME "
    fi
done <"$VFILE"
[ -n "$V_WEBIF_SPEC" ] && VVARIABLES="V_WEBIF_SPEC $VVARIABLES"
for v_var in V_SKIN V_CUSTOM_FEATURE_PACK V_PRODUCT; do
    [ -n "$v_var" ] && VVARIABLES+="$v_var "
done

# Bail out with error
bail() {
    echo "ERROR: $@" 1>&2
    exit 1
}

# Run $@ and check return value. Bail on error.
rac() {
    #echo "$@"
    ${1+"$@"}
    RVAL=$?
    if [ $RVAL -ne 0 ]; then
        echo "ERROR($RVAL): $@" 1>&2
        exit $RVAL
    fi
}

# Returns $1st argument
pick() { shift $1; echo "$1"; }

# log report and collect errors. $1 is flag, $2 is string.
#
# Flags:
#   A = added (OK)
#   M = modified (OK?)
#   D = deleted (OK)
#   R = replaced (ERROR?)
#   S = script generated (script OK)
#   X = Error
#   W = Warning
#
report() {
    echo "$1 $2"
    case "$1" in
    X)
        RETVAL=1
        ;;
    *)
        ;;
    esac
}

# Traverse entire tree $1, ignoring SVN folders, writing file list (minus $1) to file $2
get_tree() {
    # Take out .svn and '.' & '..' entries
    (cd "$1" && find . -type d -not \( -wholename '*/.svn*' -prune \) | grep -v '^\.$\|~$' ) >"$2"
    (cd "$1" && find . -type f -not \( -wholename '*/.svn*' -prune \) | grep -v '^\.$\|~$' ) >>"$2"
    (cd "$1" && find . -type l -not \( -wholename '*/.svn*' -prune \) | grep -v '^\.$\|~$' ) >>"$2"
}

# return $1 file hash
fhash() {
    H=$(md5sum "$1")
    H=$(pick 1 $H)
    if [ -z "$H" ]; then
        echo "File $1 doesn't exist" 1>&2
        exit 1
    fi
    echo "$H"
}

# Attempts to copy file $1/$3 to $2/$3
#
# Checks target existence and contents, deals with symlinks
#
filecp() {
    local SRC="$1"   # Source base dir
    local DST="$2"   # Destination base dir
    local file="$3" # file name (including subdirs)

    if [ -L "$SRC/$file" ]; then
        if [ -L "$DST/$file" ]; then
            report M "[$SRC] $file"
        elif [ -f "$DST/$file" ]; then
            report X "[$SRC] $file   (Tried to replace file with link)"
        else
            report A "[$SRC] $file   (Added)"
        fi
        rac rm -f "$DST/$file"
        rac ln -fs "$(readlink "$SRC/$file")" "$DST/$file"
        return
    fi

    if [ ! -f "$SRC/$file" ]; then
        report X "[$SRCT] $file   (unknown type)"
    fi
    # It's a normal file

    if [ -L "$DST/$file" ]; then
        report X "[$SRC] $file   (Tried to replace link with file)"
    elif [ -f "$DST/$file" ]; then
        H0=$(fhash "$SRC/$file")
        H1=$(fhash "$DST/$file")
        if [ "$H0" == "$H1" ]; then
            report R "[$SRC] $file   (Replaced with identical file)"
        else
            report M "[$SRC] $file   (Replaced with modified file)"
        fi
    else
        report A "[$SRC] $file   (Added)"
    fi
    rac rm -f "$DST/$file"
    rac cp "$SRC/$file" "$DST/$file"
}

# Copies contents from directory $1 to directory $2, maintaining tree structure
dircopy() {
    SRC="$1"
    DST="$2"
    get_tree "$SRC" "$FLIST"
    while read file; do
        case $file in ./_VARIANTS*) continue;; esac
        if [ -d "$SRC/$file" ]; then
            rac mkdir -p "$DST/$file"
            continue
        else
            rac mkdir -p "$(dirname "$DST/$file")"
        fi
        filecp "$SRC" "$DST" "$file"
    done <"$FLIST"
}

CPOP=dircopy

echo "### Processing common"
$CPOP "$SRCDIR" "$TGTDIR"

# Deal with variant tree
for NAME in $VVARIABLES; do
    eval VALUE=\$$NAME
    if [ ! -d "$SRCDIR/_VARIANTS/$NAME" ]; then
        continue
    fi
    echo "### Processing _VARIANTS/$NAME ($VALUE)"
    if [ -d "$SRCDIR/_VARIANTS/$NAME/$VALUE" ]; then
        $CPOP "$SRCDIR/_VARIANTS/$NAME/$VALUE" "$TGTDIR"
    fi
done

rm -f $FLIST

if [ $RETVAL != 0 ]; then
    echo 1>&2
    echo "fsextra install has failed. See log above for error flags (XYZ) or messages" 1>&2
    echo 1>&2
    exit $RETVAL
fi

exit $RETVAL
