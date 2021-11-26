#!/bin/sh
# Copyright (C) 2019 NetComm Wireless Limited.
#
# This script generates or remove CpiSignatureData object used in SAS registration
#

nof=${0##*/}
source /lib/utils.sh

# location of CpiSignatureData object
CpiSignatureData="sas.config.cpiSignatureData"

if [ "${1}" == "deregister" ] || [ "${1}" == "force_deregister" ]; then
  rdb_set "${CpiSignatureData}" ""
  #trigger sas client in the magpie
  rdb_set "sas.registration.cmd" ${1}
  logNotice "${CpiSignatureData} removed"
  exit 0
fi

# cpi key is required to sign the signature data,
# it is expected to be (temporary) provided by installation tool
secret=$(rdb_get sas.config.cpiKeyFile)
if [ ! -e "$secret" ]; then
  logErr "No CPI key to sign cpiSignatureData"
  exit 1
fi

sn=$(rdb_get sas.config.cbsdSerialNumber)
if [ "$sn" = "" ]; then
  logErr "the serial number is not set"
  exit 2
fi

# collect installation data from RDB vars
fccid=$(rdb_get sas.config.fccid)
cbsdSerialNumber=$(rdb_get sas.config.cbsdSerialNumber)
latitude=$(rdb_get sas.antenna.latitude)
longitude=$(rdb_get sas.antenna.longitude)
height=$(rdb_get sas.antenna.height)
heightType=$(rdb_get sas.antenna.height_type)
eirpCapability=$(rdb_get sas.antenna.eirp_cap)
antennaAzimuth=$(rdb_get sas.antenna.azimuth)
antennaDowntilt=$(rdb_get sas.antenna.downtilt)
antennaGain=$(rdb_get sas.antenna.gain)
antennaBeamwidth=$(rdb_get sas.antenna.beam_width)
antennaModel=$(rdb_get sas.antenna.model)
indoorDeployment=$(rdb_get sas.config.indoorDeployment)
cpiId=$(rdb_get sas.config.cpiId)
cpiName=$(rdb_get sas.config.cpiName)
installCertificationTime=$(rdb_get sas.config.installCertificationTime)
payload=$(cat <<EOF
{
"fccId" : "$fccid",
"cbsdSerialNumber": "$cbsdSerialNumber",
"installationParam": {
 "latitude"        : $latitude,
 "longitude"       : $longitude,
 "height"          : $height,
 "heightType"      : "$heightType",
 "eirpCapability"  : $eirpCapability,
 "antennaAzimuth"  : $antennaAzimuth,
 "antennaDowntilt" : $antennaDowntilt,
 "antennaGain"     : $antennaGain,
 "antennaBeamwidth": $antennaBeamwidth,
 "antennaModel"    : "$antennaModel",
 "indoorDeployment": $indoorDeployment
 },
"professionalInstallerData": {
 "cpiId" : "$cpiId",
 "cpiName": "$cpiName",
 "installCertificationTime": "$installCertificationTime"
 }
}
EOF
)

b64url() {
  openssl base64 -A | tr '+/' '-_' | tr -d '='
}

sign_ecdsa() {
  sigFile=$(mktemp)
  openssl dgst -binary -sha256 -sign $secret -out $sigFile
  if openssl asn1parse -inform DER < $sigFile | grep '37:d=1' >/dev/null; then
    { dd bs=1 skip=5 count=32 2>/dev/null ; tail -c32 ; } < $sigFile
  else
    { dd bs=1 skip=4 count=32 2>/dev/null ; tail -c32 ; } < $sigFile
  fi
  rm $sigFile
}

sign_rsa() {
  openssl dgst -binary -sha256 -sign $secret
}

payload_base64=$(echo -n "${payload}"| openssl base64 -A)
if openssl ec -check -in $secret >/dev/null 2>&1; then
  header=$(echo -n '{"typ":"JWT","alg":"ES256"}' | openssl base64 -A)
  signature=$(echo -n "$header.$payload_base64" | sign_ecdsa | b64url)
else
  header=$(echo -n '{"typ":"JWT","alg":"RS256"}' | openssl base64 -A)
  signature=$(echo -n "$header.$payload_base64" | sign_rsa | b64url)
fi

if [ -z "$CpiSignatureData" ]; then
  logErr "cpiSignatureData location is not set"
  exit 3
fi

# save the cpi signature as a json string
rdb_set "$CpiSignatureData" '{"protectedHeader" : "'${header}'","encodedCpiSignedData" : "'${payload_base64}'","digitalSignature" : "'${signature}'"}'

logNotice "${CpiSignatureData} generated"
#trigger sas client in the FWA
rdb_set "sas.registration.cmd" ${1}
exit 0
