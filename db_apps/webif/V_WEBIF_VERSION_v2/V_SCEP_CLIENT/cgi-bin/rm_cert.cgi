#!/bin/sh

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
       exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
	exit 254
fi

rm -f /etc/ipsec.d/certs/scep.crt
rm -f /etc/ipsec.d/private/scep.key
rdb_set service.scep.cert_issuer
rdb_set service.scep.cert_subject
rdb_set service.scep.cert_common_name
rdb_set service.scep.cert_email
rdb_set service.scep.cert_valid_from
rdb_set service.scep.cert_valid_until
rdb_set service.scep.cert_scep_uri

rdb_set service.scep.status ""

cat << EOF
Status: 200 OK
Cache-Control: no-cache

EOF
