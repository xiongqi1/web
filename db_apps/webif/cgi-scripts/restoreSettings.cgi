#!/bin/sh
#
# import save settings used by landling.html at factory reset event
# the factory password is verified before proceeding with restoring
#
# Copyright (C) 2018 NetComm Wireless Limited.
#
if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
    # ignore cases when server side does not use session ID (eg. factory reset)
   if [ -n "${sessionid}" ]; then
       exit 0
   fi
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
   exit 254
fi

. /lib/utils.sh
nof=${0##*/}

htmlWrite() {
   echo -n -e "$@"
}

# splits CGI query into var="value" strings
cgi_split() {
   echo "$1" | awk 'BEGIN{
      hex["0"] =  0; hex["1"] =  1; hex["2"] =  2; hex["3"] =  3;
      hex["4"] =  4; hex["5"] =  5; hex["6"] =  6; hex["7"] =  7;
      hex["8"] =  8; hex["9"] =  9; hex["A"] = 10; hex["B"] = 11;
      hex["C"] = 12; hex["D"] = 13; hex["E"] = 14; hex["F"] = 15;
   }
   {
      n=split ($0,EnvString,"&");
      for (i = n; i>0; i--) {
         z = EnvString[i];
         x=gsub(/\=/,"=\"",z);
         x=gsub(/\+/," ",z);
         while(match(z, /%../)){
            if(RSTART > 1)
               printf "%s", substr(z, 1, RSTART-1)
            printf "%c", hex[substr(z, RSTART+1, 1)] * 16 + hex[substr(z, RSTART+2, 1)]
            z = substr(z, RSTART+RLENGTH)
         }
         x=gsub(/$/,"\"",z);
         print z;
      }
   }'
}

qlist=`cgi_split "$QUERY_STRING"`

split() {
   shift $1
   echo "$1"
}

# parse for expecting CGI parameters
for V in $qlist; do
   VAR="CGI_PARAM_$V"
   NAME=$(echo $VAR|cut -f1 -d"=")

   if [ "$NAME" = "CGI_PARAM_factory" ]; then
      CGI_PARAM_factory=$(echo $VAR|cut -f2- -d"="|base64 -d)
   fi

   if [ "$NAME" = "CGI_PARAM_file" ]; then
      CGI_PARAM_file=$(echo $VAR|cut -f2- -d"=")
   fi

   if [ "$NAME" = "CGI_PARAM_pass" ]; then
      CGI_PARAM_pass=$(echo $VAR|cut -f2- -d"=")
   fi
done

htmlWrite "Status: 200\n"
htmlWrite "Content-type: text/json\n"
htmlWrite "Cache-Control: no-cache\n"
htmlWrite "Connection: keep-alive\n\n"

cgiresult=255     # general failure
# check factory password
if [ "$CGI_PARAM_factory" != "$(rdb get admin.user.root)" ]; then
   # use 249 for factory password mismatch, see other error codes in restoreSettings.sh
   cgiresult=249
else
  # do restore save settings
  /usr/bin/restoreSettings.sh "$CGI_PARAM_file" "$CGI_PARAM_pass" > /dev/null 2> /dev/null
  cgiresult=$?
fi

logNotice "file:$CGI_PARAM_file, cgiresult:$cgiresult"

cat << EOF
{
   "cgiresult":$cgiresult
}
EOF
