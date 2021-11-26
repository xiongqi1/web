#!/bin/sh
###########################################################################
#   Copyright (C) 2004-2010 by Dimark Software Inc.                       #
#   support@dimark.com                                                    #
###########################################################################

killall -9 dimclient
CDIR="./tmp"

homedir=`pwd`
echo homedir = $homedir

# Check to see if $CDIR exists
if   [ -d $CDIR ] ; then
   echo -n ""
elif [ -e $CDIR ] ; then
   rm -rf $CDIR
   mkdir  $CDIR
else
   mkdir  $CDIR
fi

if cd $CDIR ; then
    echo cd $CDIR
else
    echo could not cd to $CDIR
    exit 1
fi

rm -f *.dat tmp.param

#for dir in data filetrans options parameter ; do
#
#  if [ -d $dir ] ; then
#     rm -f ${dir}/*
#  elif [ -e $dir ] ; then
#     rm -rf $dir
#     mkdir $dir   > /dev/null 2>&1
#  else
#     mkdir $dir   > /dev/null 2>&1
#  fi
#  mkdir $dir      > /dev/null 2>&1
#
#done

cd $homedir
rm -f *.log

#./conv-util data-model.xml > tmp/tmp.param
cp lua/*.lua tmp/
cp -r lua/handlers tmp/
cp -r lua/objectlua tmp/
cp lua/luardb.so tmp/

./dimclient
#until ./dimclient
#do
#    echo Go to the system to boot. Start to boot...
#done

#end
