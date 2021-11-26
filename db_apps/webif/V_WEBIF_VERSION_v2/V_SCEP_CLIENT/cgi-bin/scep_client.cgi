#!/bin/sh
# process SCEP actions
# currently support:
# - getca
# - load CA certificates downloaded in "getca" process
# - get enrolment status and certificate
# return result in JSON format in HTTP response
# return code is indicated in JSON variable "rval": 0 for success; otherwise error

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
	exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	exit 254
fi

. /lib/utils.sh
nof=${0##*/}
rval=0
action=
scepData=

scep_dir="/etc/ipsec.d/cacerts"
scep_ca_prefix="scep_ca_cert"

# print result in JSON format in HTTP response
print_output() {
	cat << EOF
Status: 200 OK
Content-type: application/json
Cache-Control: no-cache

{
    "rval": "${rval}",
    "scepData": [ ${scepData} ]
}
EOF
}

# get CA
scep_get_ca() {
	# validate input
	if ! echo $scepUrl | grep -qEi "^https?://[^[:blank:]/$.?#]+[^[:blank:]]*$"; then
		rval=1
		print_output
		return
	fi
	# write config file
	scep_config_file="/tmp/scepconfig$$.conf"
	cat << EOF > $scep_config_file
URL $scepUrl
CACertFile ${scep_dir}/${scep_ca_prefix}
EOF
	# create directory if necessary
	mkdir -p $scep_dir
	# get CA and return results
	sscep getca -f $scep_config_file -v -d 2>&1 | logNotice
	rval=$?
	rm -f $scep_config_file
	if [ "$rval" = "0" ]; then
		scep_load_ca
	else
		print_output
	fi
}

# load CA certificates which have already downloaded from getca process
scep_load_ca() {
	local idx=0
	local cert_data=
	for f in $(ls ${scep_dir}/${scep_ca_prefix}* 2>/dev/null); do
		# extract some attributes from the certificate
		# escape double quotes as the data will be written into JSON
		if cert_data=$(openssl x509 -in $f -subject -dates -issuer -noout 2>/dev/null | sed -r -e 's/\\/\\\\/g' -e 's/\"/\\\"/g' \
				-e 's/^subject=(.+)$/, "subject": "\1"/' \
				-e 's/^notBefore=(.+)$/, "notBefore": "\1"/' \
				-e 's/^notAfter=(.+)$/, "notAfter": "\1"/' \
				-e 's/^issuer=(.+)$/, "issuer": "\1"/'); then
			test $idx -gt 0 && scepData="${scepData}, "
			scepData="${scepData}{ \"filename\": \"$(basename $f)\" $cert_data }"
			idx=$((idx+1))
		fi
	done
	print_output
}

rdb_get_escape_quotes() {
	rdb_get "$1" | sed -r -e 's/\\/\\\\/g' -e 's/\"/\\\"/g'
}

# get enrolment status and certificate
scep_get_enrolment() {
	local status="$(rdb_get service.scep.status)"
	local cert_issuer=
	local cert_subject=
	local cert_common_name=
	local cert_valid_from=
	local cert_valid_until=
	local cert_email=
	local cert_scep_uri=

	if [ -z "$status" -o "$status" = "existing" -o "$status" = "enrolled" ]; then
		cert_issuer="$(rdb_get_escape_quotes service.scep.cert_issuer)"
		cert_subject="$(rdb_get_escape_quotes service.scep.cert_subject)"
		cert_common_name="$(rdb_get_escape_quotes service.scep.cert_common_name)"
		cert_valid_from="$(rdb_get_escape_quotes service.scep.cert_valid_from)"
		cert_valid_until="$(rdb_get_escape_quotes service.scep.cert_valid_until)"
		cert_email="$(rdb_get_escape_quotes service.scep.cert_email)"
		cert_scep_uri="$(rdb_get_escape_quotes service.scep.cert_scep_uri)"
	fi

	scepData="{ \"enrolmentStatus\": \"$status\", \
\"certIssuer\": \"$cert_issuer\", \
\"certSubject\": \"$cert_subject\", \
\"certCommonName\": \"$cert_common_name\", \
\"certValidFrom\": \"$cert_valid_from\", \
\"certValidUntil\": \"$cert_valid_until\", \
\"certEmail\": \"$cert_email\", \
\"certScepUri\": \"$cert_scep_uri\" }"
	print_output
}

parse_post_data "scepAction scepUrl"

case "$scepAction" in
	getCa)
		scep_get_ca
		;;
	loadCa)
		scep_load_ca
		;;
	getEnrolmentStatus)
		scep_get_enrolment
		;;
esac
