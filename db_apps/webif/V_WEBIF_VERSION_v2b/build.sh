#!/bin/bash
#
# Builds V2B code.  Refer to WebUI programmers guide page in the NetComm wiki.
#
# If any of the nodejs stages fails then the script aborts with an error code.
#
# Options:
#  -h, --help          display help text and quit
#  --debug             debug is turned on when --debug is the first argument
#
# Copyright (C) 2018 NetComm Wireless Limited.

echo "*************************************************************************"
echo "*************************************************************************"
echo "BEGIN v2b compile"
echo "*************************************************************************"
echo "*************************************************************************"


ROOT_DIR=$(pwd)
echo "v2b root dir $ROOT_DIR"
BuildDir=$ROOT_DIR/build
PageDir=$BuildDir/pages
GenDir=$BuildDir/gen

rm -rf $BuildDir
mkdir -p $PageDir
mkdir -p $GenDir

# display help stuff for caller if requested
[ $# = 1 ] && [ "$1" = --help -o "$1" = -h ] && sed -n '/^[^#]/q;1!s/^#//p' $0 && exit

# turn on debugging (of shell script) if requested
[ "$1" = --debug ] && shift && set -x

die() {
    ret_val=$?
    echo "ERROR $ret_val: $@">&2
    exit $ret_val
}

try() {
    $@ || die running "$@"
}

echo "Building WebUI v2b"
nodejs -v || die "nodejs missing."
tsc -v || die "Typescript missing."
env | grep ^V_LA
env | grep ^V_T

# The C preprocessor is used for V_ variable customisation
preprocess() {
    cat $ROOT_DIR/src/ntc_preproc.h $1 > preprocess.this
    gcc -w -E -x c -P -nostdinc $3 -DWEBUI_V2B $MENUFLAG preprocess.this>$2
    rm preprocess.this
}

# create translation file xlat.js
try nodejs createTranslationFiles.js ../../Internationalization/strings.csv  $INSTALLDIR

echo "Making UI Framework Javascript for device - NTC_UI.js"

# This pulls all the framework files into a single TS file
# Then compile the typescript into javascript and create a types file for the pages
preprocess src/NTC_UI.js $BuildDir/NTC_UI.ts
try tsc --removeComments --declaration --outFile ntc_ui2.js $BuildDir/NTC_UI.ts
mv ntc_ui2.d.ts $BuildDir
# Some legacy JS is added to the framework
preprocess ../V_WEBIF_VERSION_v2/common/net_util.js $BuildDir/net_util.js
preprocess ../V_WEBIF_VERSION_v2/common/util.js $BuildDir/util.js
cat ntc_ui2.js $BuildDir/net_util.js $BuildDir/util.js > $INSTALLDIR/www/js/NTC_UI.js
rm ntc_ui2.js

# include variants.sh
VFILE=$HBASE/variant.sh

POSTVARS="V_SKIN V_CUSTOM_FEATURE_PACK V_PRODUCT"

# Extract variant variables
# $1 : source directory
extract_vvariable() {
    local SRCDIR="$1"
    VVARIABLES=""
    while read line; do
        if echo "$line" | grep -v -q '.\+=.\+'; then
            continue
        fi
        NAME=$(echo "$line" | cut -d = -f 1)
        VALUE=$(echo "$line" | cut -d = -f 2|sed "s/'//g")
        if [ -d "$SRCDIR/$NAME/$VALUE" ]; then
            eval $NAME=$VALUE
            VVARIABLES+="$NAME "
        fi
    done <"$VFILE"
}

# For each page defined
pushd ./src
echo "Preprocess .js files"

# $1 : directory name where .js files reside
preprocess_js_file() {
    test -d "$1" || return
    for page in $1/*js
    do
        local base=$(basename $page .js)
        # to get around the pain of node's require() generate a single
        # preprocessed JS file for Nodejs to run
        cat ../Compile.js $page ../genHtmlLua.js > gen$base.pts
        echo "    preprocess gen$base.pts $GenDir/gen$base.ts -DCOMPILE_WEBUI"
        preprocess gen$base.pts $GenDir/gen$base.ts "-DCOMPILE_WEBUI"
        rm gen$base.pts

        # Now generate the page js that is added to the template html
        preprocess $page $PageDir/$base.ts
    done
}

echo "## process default pages"
preprocess_js_file pages

# Alphabetical-Ordered variables
echo "## process Alphabetical-Ordered variables"
extract_vvariable pages/variants
for VVAR in $VVARIABLES; do
    run_now=true
    for ORDERVAR in $POSTVARS; do
        if [ "$VVAR" = "$ORDERVAR" ]; then
            run_now=false
            break
        fi
    done
    if $run_now; then
        eval VAL=\$$VVAR
        preprocess_js_file "pages/variants/$VVAR/$VAL"
    fi
done

echo "## process Post-Ordered variables"
# Post-Ordered variables
for VVAR in $POSTVARS; do
    eval VAL=\$$VVAR
    preprocess_js_file "pages/variants/$VVAR/$VAL"
done

echo "Compile pages"
try tsc --removeComments --outDir $PageDir/ $BuildDir/ntc_ui2.d.ts $PageDir/*.ts
echo "Compile page generators"
try tsc --removeComments --outDir $GenDir/ $GenDir/*.ts

# $1 : directory name where .js files reside
compile_pages() {
    test -d "$1" || return
    for page in $1/*js
    do
        local base=$(basename $page .js)
        rm -f "$INSTALLDIR/www/$base.htmlv2b"

        echo "Create html and Lua scripts for $base"
        #rm $GenDir/gen$base.ts

        #rm $PageDir/$base.ts
        try nodejs $GenDir/gen$base.js $INSTALLDIR $base $PageDir/$base.js ".htmlv2b"
        #rm $PageDir/$base.js $GenDir/gen$base.js

        # Now rename the v2 html file to html and create a link to our v2b html file
        if [ -e "$INSTALLDIR/www/$base.htmlv2b" ]; then
            echo ".html created for $base"
            rm -f "$INSTALLDIR/www/$base.html"
        fi
    done
}

echo "## compile default pages"
compile_pages pages

# Alphabetical-Ordered variables
echo "## compile Alphabetical-Ordered variables"
for VVAR in $VVARIABLES; do
    run_now=true
    for ORDERVAR in $POSTVARS; do
        if [ "$VVAR" = "$ORDERVAR" ]; then
            run_now=false
            break
        fi
    done
    if $run_now; then
        eval VAL=\$$VVAR
        compile_pages "pages/variants/$VVAR/$VAL"
    fi
done

echo "## compile Post-Ordered variables"
# Post-Ordered variables
for VVAR in $POSTVARS; do
    eval VAL=\$$VVAR
    compile_pages "pages/variants/$VVAR/$VAL"
done

popd

# preprocess Lua cgi files
echo "Preprocess .cgi files"

# $1 : directory name where .js files reside
preprocess_cgi_file() {
    test -d "$1" || return
    for cgiFile in $1/*
    do
        local newCgiFile=$(basename $cgiFile)
        # Do not preprocess json files which cause compile error
        local objLua=$(echo $newCgiFile | grep "^obj.*.lua")
        if [ -n "$objLua" ]; then
            echo "      preprocess $cgiFile..."
            preprocess $cgiFile $INSTALLDIR/www/cgi-bin/$newCgiFile
        else
            echo "      cp $1/$newCgiFile $INSTALLDIR/www/cgi-bin/$newCgiFile"
            cp $1/$newCgiFile $INSTALLDIR/www/cgi-bin/$newCgiFile
        fi
    done
}

echo "## preprocess default cgi files"
preprocess_cgi_file src/cgi-bin

# Alphabetical-Ordered variables
echo "## preprocess Alphabetical-Ordered variables"
extract_vvariable src/cgi-bin/variants
for VVAR in $VVARIABLES; do
    run_now=true
    for ORDERVAR in $POSTVARS; do
        if [ "$VVAR" = "$ORDERVAR" ]; then
            run_now=false
            break
        fi
    done
    if $run_now; then
        eval VAL=\$$VVAR
        preprocess_cgi_file "src/cgi-bin/variants/$VVAR/$VAL" 2>/dev/null
    fi
done

echo "## preprocess Post-Ordered variables"
# Post-Ordered variables
for VVAR in $POSTVARS; do
    eval VAL=\$$VVAR
    preprocess_cgi_file "src/cgi-bin/variants/$VVAR/$VAL"
done

# This deletes the v2 files that were obsoleted by the v2b files
rm -rf $INSTALLDIR/www/obsolete

# merge per-variant config files
./install_extension.sh $HBASE/variant.sh src/cgi-bin/variants $INSTALLDIR/www/cgi-bin
