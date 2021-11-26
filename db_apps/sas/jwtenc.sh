#!/bin/sh

#
# This script signs the insatll data using the installer's private key.
#

# this is the installer key, which we will use to sign.
# We are never to store this on the device.
secret='cpicertprivkey.key'

# Static header fields.
header='{"alg":"RS256"}'


fccid=$(rdb_get sas.config.fccid)
cbsdSerialNumber=$(rdb_get sas.config.cbsdSerialNumber)
latitude=$(rdb_get sas.config.latitude)
longitude=$(rdb_get sas.config.longitude)
height=$(rdb_get sas.config.height)
heightType=$(rdb_get sas.config.heightType)
eirpCapability=$(rdb_get sas.config.eirpCapability)
antennaAzimuth=$(rdb_get sas.config.antennaAzimuth)
antennaDowntilt=$(rdb_get sas.config.antennaDowntilt)
antennaGain=$(rdb_get sas.config.antennaGain)
antennaBeamwidth=$(rdb_get sas.config.antennaBeamwidth)
antennaModel=$(rdb_get sas.config.antennaModel)
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

base64_encode()
{
	input=${1:-$(</dev/stdin)}
	printf '%s' "${input}" | openssl enc -base64 -A
}

sha256_sign()
{
	input=${1:-$(</dev/stdin)}
	printf '%s' "${input}" | openssl dgst -binary -sha256 -sign "${secret}"
}

header_base64=$(base64_encode "${header}")
payload_base64=$(base64_encode "${payload}")

header_payload="${header_base64}.${payload_base64}"
signature=$(base64_encode "$(sha256_sign "${header_payload}")")

echo '{
      "protectedHeader" : "'${header_base64}'",
      "encodedCpiSignedData" : "'${payload_base64}'",
      "digitalSignature" : "'${signature}'"
}'
