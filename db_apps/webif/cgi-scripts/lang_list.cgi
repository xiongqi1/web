#!/bin/sh
echo -e 'Content-type: text/html\n'

echo "var lang_en=0; var lang_zhtw=0; var lang_zhcn=0; var lang_fr=0; var lang_ar=0;"
v=`ls -d ../lang/*/ |sed 's/..\/lang\//var lang_/'`
echo $v |sed 's/[\/]/=1;/g'

