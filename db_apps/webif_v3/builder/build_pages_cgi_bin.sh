#!/bin/bash
# Copyright (c) 2020 Casa Systems
#
# Making UI Framework Javascript
#
# Expects the following environment variables to be set
#
# TEMPINSTALL = Temporary install directory
# TEMPBUILD   = Temporary build directory
# CFLAGS      = CFLAGS for C-preprocessing
# SRCDIR      = Source directory
# BUILDERDIR  = Builder directory
# HBASE       = Platform history directory

BuildDir=$TEMPBUILD
PageDir=$BuildDir/pages
GenDir=$BuildDir/gen

die() {
    ret_val=$?
    echo "ERROR $ret_val: $@">&2
    exit $ret_val
}

try() {
    $@ || die running "$@"
}

# The C preprocessor is used for V_ variable customisation
preprocess() {
    cat $SRCDIR/common/ntc_preproc.h $1 > $BuildDir/preprocess.this
    gcc -w -E -x c -P -nostdinc -I ./ $3 $CFLAGS $BuildDir/preprocess.this>$2
    rm $BuildDir/preprocess.this
}

echo "Making UI Framework Javascript for device - NTC_UI.js"

# This pulls all the framework files into a single TS file
# Then compile the typescript into javascript and create a types file for the pages
preprocess $SRCDIR/common/NTC_UI.js $BuildDir/NTC_UI.ts
try npx tsc --removeComments --declaration --outFile $BuildDir/ntc_ui2.js $BuildDir/NTC_UI.ts
# Some legacy JS is added to the framework
preprocess $SRCDIR/common/net_util.js $BuildDir/net_util.js
preprocess $SRCDIR/common/util.js $BuildDir/util.js
cat $BuildDir/ntc_ui2.js $BuildDir/net_util.js $BuildDir/util.js > $TEMPINSTALL/www/js/NTC_UI.js
rm $BuildDir/ntc_ui2.js

# include variants.sh
VFILE=$HBASE/variant.sh

$BUILDERDIR/install_files_v_vars.sh $VFILE $SRCDIR/pages $TEMPBUILD/src_pages

# For each page defined
echo "Preprocess .js files"

# $1 : directory name where .js files reside
preprocess_js_file() {
    test -d "$1" || return
    for page in $1/*.js
    do
        local base=$(basename $page .js)
        # to get around the pain of node's require() generate a single
        # preprocessed JS file for Nodejs to run
        cat $BUILDERDIR/Compile.ts $page $BUILDERDIR/genHtmlLua.ts $TEMPBUILD/theme_gen/genHeadTags.js > $TEMPBUILD/gen$base.pts
        echo "    preprocess gen$base.pts $GenDir/gen$base.ts -DCOMPILE_WEBUI"
        preprocess $TEMPBUILD/gen$base.pts $GenDir/gen$base.ts "-DCOMPILE_WEBUI"
        rm $TEMPBUILD/gen$base.pts

        # Now generate the page js that is added to the template html
        preprocess $page $PageDir/$base.ts
    done
}

echo "## process pages"
preprocess_js_file $TEMPBUILD/src_pages

echo "Compile pages"
try npx tsc --removeComments --outDir $PageDir/ $BuildDir/ntc_ui2.d.ts $PageDir/*.ts
echo "Compile page generators"
try npx tsc --removeComments --outDir $GenDir/ $GenDir/*.ts

# $1 : directory name where .js files reside
compile_pages() {
    test -d "$1" || return
    for page in $1/*js
    do
        local base=$(basename $page .js)
        rm -f "$TEMPINSTALL/www/$base.html"

        echo "Create html and Lua scripts for $base"
        #rm $GenDir/gen$base.ts

        #rm $PageDir/$base.ts
        try nodejs $GenDir/gen$base.js $TEMPINSTALL $base $PageDir/$base.js ".html"
        #rm $PageDir/$base.js $GenDir/gen$base.js

    done
}

echo "## compile default pages"
compile_pages $TEMPBUILD/src_pages

# preprocess Lua cgi files
echo "Preprocess .cgi files"

$BUILDERDIR/install_files_v_vars.sh $VFILE $SRCDIR/cgi-bin $TEMPBUILD/src_cgi_bin

# $1 : directory name where .js files reside
preprocess_cgi_file() {
    test -d "$1" || return
    for cgiFile in $1/*
    do
        if [ -f "$cgiFile" ]; then
            local newCgiFile=$(basename $cgiFile)
            # Do not preprocess json files which cause compile error
            local objLua=$(echo $newCgiFile | grep "^obj.*.lua")
            if [ -n "$objLua" ]; then
                echo "      preprocess $cgiFile..."
                preprocess $cgiFile $TEMPINSTALL/www/cgi-bin/$newCgiFile
            else
                echo "      cp $1/$newCgiFile $TEMPINSTALL/www/cgi-bin/$newCgiFile"
                cp $1/$newCgiFile $TEMPINSTALL/www/cgi-bin/$newCgiFile
            fi
        fi
    done
}

echo "## preprocess default cgi files"
preprocess_cgi_file $TEMPBUILD/src_cgi_bin
