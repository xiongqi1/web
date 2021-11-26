#!/bin/sh
#
# Download the WLAN STA WPA PKI key and certificate files.
#
# We are expecting stdin to look something like this:
#   -----------------------------38026035515432338451676012903^M
#   Content-Disposition: form-data; name="KeyFile"; filename="private_key.key"^M
#   Content-Type: application/pgp-keys^M
#   ^M
#   Line 1
#   ...
#   line N
#   ^M
#   -----------------------------38026035515432338451676012903--^M

nof=`basename $0`           # program name
clientNum="0"               # Client radio instance
savedApNum="0"              # The saved AP profile instance
msgFilename=""              # Name of downloaded file from message header.
tmpFile="/tmp/$nof.tmp"     # Temporary file

test -z $(echo $PATH | grep local) && PATH="/usr/local/bin:/usr/local/sbin:"$PATH

# To log messages to syslog, prefixed by script name.
log() {
  logger -t $nof -- $@
}

sendHttpOk() {
  cat <<EOF
Status: 200
Content-Type: text/plain
Cache-Control: no-cache

EOF
}

sendBadHdrAndExit() {
  sendHttpOk
  cat <<EOF
{
  "result":"bad message header"
}
EOF
  exit 0
}

# Ignore if session not current
if [ -z "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ] ;then
  sendHttpOk
  cat <<EOF
{
  "result":"session timed out"
}
EOF
  exit 0
fi

# Map the command to the RDB variable and destination file.
#   Note: We are only expecting a single argument.
action=""
case "$QUERY_STRING" in
"cmd=GetAllMetadata")
  action="getAll"
  ;;
"cmd=KeyFile"|"cmd=DeleteKeyFile")
  rdbSuffix="wpa_pki_client_private_key"
  ;;
"cmd=CaCertificate"|"cmd=DeleteCaCertificate")
  rdbSuffix="wpa_pki_ca_certificate"
  ;;
"cmd=ClientCertificate"|"cmd=DeleteClientCertificate")
  rdbSuffix="wpa_pki_client_certificate"
  ;;
*)
  log "Unknown cmd: $QUERY_STRING"
  sendHttpOk
  cat <<EOF
{
  "result":"bad argument",
  "query":"$QUERY_STRING"
}
EOF
  exit 0
  ;;
esac
rdbVar="wlan_sta.$clientNum.ap.$savedApNum.$rdbSuffix"

# Determine the action requested.
if [ -z "$action" ] ;then
  if [ ${QUERY_STRING#"cmd=Delete"} != "$QUERY_STRING" ] ;then
    action="delete"
  elif [ ${QUERY_STRING#"cmd=Get"} != "$QUERY_STRING" ] ;then
    action="get"
  else
    action="download"
  fi
fi

readUntilBlank() {
	while read line; do
    if ! echo "$line" | tr -d '\r' | grep -q "^$" ;then
			continue
		fi
    return 0
  done
  return 1
}

discardHdr() {
  while read line; do
    if echo "$line" | grep -q "Content-Disposition: .* filename=" ;then
      msgFilename=$(echo $line | sed -r "s/^.+filename=\"(.+)\".*/\1/")
      continue
    elif echo "$line" | grep -q "Content-Type: " ;then
      readUntilBlank || return 1
      return 0
    else
      continue
    fi
  done
  return 1
}

triggerTemplate()
{
  rdb set wlan_sta.${clientNum}.trigger 1
}

downloadFile() {
  local boundary
  local tmp
  local uploaded
  local size

  # Get the content boundary.
  # We expect $CONTENT_TYPE to look something like this:
  #   multipart/form-data; boundary=---------------------------3802603551543233845167601290
  boundary=${CONTENT_TYPE#*boundary=}

  # Filter and write the data to the RDB variable, and tmp file.
  # Notes:
  #   - sed filter:
  #     - it removes the end boundary line
  #     - it allows for extra '-' before the boundary and excess chars after it
  #   - Use "rdb set" instead of "rdb_set" to handle leading hyphens better.
  discardHdr || { sendBadHdrAndExit; }
  rdb set $rdbVar "$(cat 2> /dev/null | sed -r "/^-*${boundary}.*/d" | tee $tmpFile)"

  # Set the metadata RDBs
  rdb_set -p $rdbVar.filename "$msgFilename"
  uploaded="$(date +%d-%m-%Y\ %H:%M:%S)"
  rdb_set -p $rdbVar.uploaded "$uploaded"
  size=$(stat -c %s $tmpFile)
  rdb_set -p $rdbVar.size "$size"

  triggerTemplate
  sendHttpOk
  sendFileMetadata "$msgFilename" "$uploaded" "$size" "$tmpFile" "$rdbSuffix"
  /bin/rm -f $tmpFile
}

deleteFile() {
  rdb unset $rdbVar

  # Remove the metadata RDBs
  rdb_del $rdbVar.filename
  rdb_del $rdbVar.uploaded
  rdb_del $rdbVar.size

  triggerTemplate

  sendHttpOk
  cat <<EOF
{
  "result":"ok"
}
EOF
}

# Returns an errorCode
isValidCertificate() {
  local jsonObjName=$1
  local file=$2
  local errorCode

  case "$jsonObjName" in
  "wpa_pki_ca_certificate"|"wpa_pki_client_certificate")
    openssl x509 -in $file -noout 2> /dev/null
    errorCode=$?
    ;;
  "wpa_pki_client_private_key")
    grep -q "BEGIN ENCRYPTED PRIVATE KEY" $file && grep -q "END ENCRYPTED PRIVATE KEY" $file
    errorCode=$?
    if [ "$errorCode" = "0" ] ;then
      if grep -q "BEGIN CERTIFICATE" $file ;then
        errorCode=1
      else
        errorCode=0
      fi
    fi
    ;;
  *)
    errorCode=1
    ;;
  esac

  return $errorCode
}

getCertificateDetails() {
  local file=$1
  local details

  # Don't get details for private keys
  if head -1 $file | grep "BEGIN ENCRYPTED PRIVATE KEY" > /dev/null 2>&1
  then
    echo "Private key details not shown"
    return 0
  fi

  # Do some simple formatting to make it look readable in html.
  # Note: Convert newlines to <br> as they can't be used in a JSON string.
  if [ -n "$file" ] ;then
    details="$(openssl x509 -in $file -noout -text \
          -certopt no_header,no_version,no_pubkey,no_sigdump,no_aux,no_extensions \
          2> /dev/null | \
          awk '{
            gsub("Serial Number"      , "<b>&</b>");
            gsub("Signature Algorithm", "<b>&</b>");
            gsub("Issuer"             , "<b>&</b>");
            gsub("C="                 , "<br>\\&nbsp;\\&nbsp;Country=");
            gsub("ST="                , "\\&nbsp;\\&nbsp;State or Province=");
            gsub("L="                 , "\\&nbsp;\\&nbsp;Locality=");
            gsub("O="                 , "\\&nbsp;\\&nbsp;Organisation=");
            gsub("CN="                , "\\&nbsp;\\&nbsp;Name=");
            gsub("/emailAddress="     , "<br>\\&nbsp;\\&nbsp;EmailAddress=");
            gsub("Validity"           , "<b>&</b>:");
            gsub("Not (Before|After)" , "\\&nbsp;\\&nbsp;&");
            gsub("Subject"            , "<b>& \\&nbsp;(Issued to)</b>");
            gsub(",", "<br>");
            printf "%s<br>", $0;
          }' )"
    details="${details}<b>Fingerprints</b>:<br>"
    details="${details}$(openssl x509 -sha1 -in $file -noout -fingerprint 2>/dev/null | \
          awk '{ printf "%s<br>", gensub("SHA1", "\\&nbsp;\\&nbsp;&", "g"); }' )"
    details="${details}$(openssl x509 -md5 -in $file -noout -fingerprint 2>/dev/null | \
          awk '{ printf "%s<br>", gensub("MD5",  "\\&nbsp;\\&nbsp;&", "g"); }' )"
  fi
  echo $details
}

getAllFileMetadata() {
  local usersFilename
  local uploaded
  local size
  local rdbBase

  sendHttpOk
  echo "{"
  for i in "wpa_pki_client_private_key" "wpa_pki_ca_certificate" "wpa_pki_client_certificate"
  do
    rdbBase="wlan_sta.$clientNum.ap.$savedApNum.$i"
    usersFilename=$(rdb_get $rdbBase.filename)
    uploaded=$(rdb_get $rdbBase.uploaded)
    size=$(rdb_get $rdbBase.size)

    rdb get $rdbBase > $tmpFile
    sendFileMetadata "$usersFilename" "$uploaded" "$size" "$tmpFile" "$i" "yes"
    /bin/rm -f $tmpFile
    echo ","
  done

  cat <<EOF
"_notUsed_":""
}
EOF
}

sendFileMetadata() {
  local usersFilename="$1"
  local uploaded="$2"
  local size="$3"
  local certificateFile="$4"
  local jsonObjName="$5"
  local useJsonObjName="$6"
  local result
  local details

  # Set result and get certificate details if required.
  details=""
  if [ -z "$usersFilename" ] || [ ! -f "$certificateFile" ] ;then
    result="empty"
  elif ! isValidCertificate "$jsonObjName" "$certificateFile" ;then
    result="invalid"
  else
    result="ok"
    details="$(getCertificateDetails $certificateFile)"
  fi

  [ "$useJsonObjName" = "yes" ] && echo "\"$jsonObjName\":{" || echo "{"
  cat <<EOF
  "result":"$result",
  "filename":"$usersFilename",
  "size":"$size",
  "uploaded":"$uploaded",
  "details":"$details"
}
EOF
}

# Perform the requested operation
case "$action" in
"download") downloadFile            ;;
"delete")   deleteFile              ;;
"getAll")   getAllFileMetadata      ;;
esac

exit 0
