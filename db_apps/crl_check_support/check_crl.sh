#!/bin/sh
#
# Copyright (C) 2019 Casa Systems Inc
#
# Helper script to download and check CRL of a certificate
# It is to support checking CRL during TLS handshake
# Parameter:
#     $1: Application name
#     $2: Current certificate in processing chain
#     $3: Path to CA directory
#     $4: CRL download timeout
# Exit code:
#     0: OK
#     3: Unable to get CRL
#     23: Certificate is revoked
#     254: Other errors
#

nof=${0##*/} # Name of file/script.
source /lib/utils.sh

APP_NAME="$1"
CHAIN_INDEX="$2"
CA_PATH="$3"
CRL_DOWNLOAD_TIMEOUT="$4"
test -z "${CRL_DOWNLOAD_TIMEOUT}" && CRL_DOWNLOAD_TIMEOUT=10

if [ -z "${APP_NAME}" -o -z "${CHAIN_INDEX}" -o -z "${CA_PATH}" ]; then
    logNotice "Application name, chain index, and CA path must be provided"
    exit 254
fi

TMP_DIR=/tmp
TMP_PREFIX="tmp_${APP_NAME}"
cd ${TMP_DIR}

CURL_OPTS=

SINGLE_APN="0"

# Init
check_crl_init() {
    if [ ${CHAIN_INDEX} -eq 1 ]; then
        rm -f ${TMP_PREFIX}*
    fi
    if [ "${APP_NAME}" = "sas_client" ]; then
        SINGLE_APN=$(rdb_get sas.use_single_apn)
        # find the profile number of CBRS SAS APN
        local SAS_PDP_PROFILE=$(rdb_get sas.config.pdp_profile)
        local PFNO=$(getPrfNoOfPdpFunction CBRSSAS $SAS_PDP_PROFILE)
        local network_interface="$(rdb_get link.profile.${PFNO}.interface)"
        if [ -n "${network_interface}" ]; then
            CURL_OPTS="${CURL_OPTS} --interface ${network_interface} --dns-interface ${network_interface}"
            local dns_servers=
            for i in $(seq 2); do
                local server_ip="$(rdb_get link.profile.${PFNO}.dns$i)"
                if [ -n "${server_ip}" ]; then
                    if [ -n "${dns_servers}" ]; then
                        dns_servers="${dns_servers},${server_ip}"
                    else
                        dns_servers="${server_ip}"
                    fi
                fi
            done
            if [ -n "${dns_servers}" ]; then
                CURL_OPTS="${CURL_OPTS} --dns-servers ${dns_servers}"
            else
                logErr "Unable to find DNS server address. rdb_get link.profile.${PFNO}.dns1 and link.profile.${PFNO}.dns2 are empty!"
                exit 254
            fi
            logNotice "CURL_OPTS=${CURL_OPTS}"
        else
            logErr "Unable to find network interface. link.profile.${PFNO}.interface is empty!"
            exit 254
        fi
        # Don't need in Myna which has single APN for data and CBRS SAS
        SAS_TRANSMIT=$(rdb_get sas.transmit_enabled)
        if [ "$SINGLE_APN" != "1" -o "$SAS_TRANSMIT" = "0" ]; then
            # to allow downloading CRL
            logNotice "Changing iptables rules to allow downloading CRLs"
            flock /tmp/sas_non_cbrs_traffic_control /usr/bin/sas_non_cbrs_traffic_control.sh unblock
        fi
    fi
}

# Deinit
check_crl_deinit() {
    if [ "${APP_NAME}" = "sas_client" ]; then
        if [ "$SINGLE_APN" != "1" ]; then
            # restore traffic blocking
            logNotice "Restoring traffic blocking"
            flock /tmp/sas_non_cbrs_traffic_control /usr/bin/sas_non_cbrs_traffic_control.sh
        fi
    fi
}

# CRL verification is OK
check_crl_verified() {
    check_crl_deinit
    logNotice "CRL verification is OK"
    rm -f ${TMP_PREFIX}*
    exit 0
}

# CRL verification failed
check_crl_failed() {
    check_crl_deinit
    test -n "$1" && logNotice "$1"
    logNotice "CRL verification failed"
    # keep generated files to debug
    test -n "$2" && exit $2
    exit 254
}

logNotice "Checking CRL for application: ${APP_NAME}, chain index: ${CHAIN_INDEX}, CA path: ${CA_PATH}, CRL download timeout: ${CRL_DOWNLOAD_TIMEOUT}"

check_crl_init

# Check certificates and convert to PEM format
for index in $(seq ${CHAIN_INDEX}); do
    if [ ! -f "${APP_NAME}_cert_${index}.der" ]; then
        check_crl_failed "${TMP_DIR}/${APP_NAME}_cert_${index}.der does not exist"
    else
        if ! openssl x509 -inform DER -outform PEM -in "${APP_NAME}_cert_${index}.der" -out "${TMP_PREFIX}_cert_${index}.pem" >/dev/null 2>&1; then
            check_crl_failed "Failed to convert ${APP_NAME}_cert_${index}.der to PEM format"
        fi
    fi
done

# Extract CRL URI of under verification certificate, which is specified by CHAIN_INDEX
if ! openssl x509 -inform DER -in "${APP_NAME}_cert_${CHAIN_INDEX}.der" -noout -text < /dev/null 2> /dev/null > ${TMP_PREFIX}_cert_${CHAIN_INDEX}.der.parsed; then
    check_crl_failed "Parsing ${APP_NAME}_cert_${CHAIN_INDEX}.der"
fi
CRL_URI="$(cat ${TMP_PREFIX}_cert_${CHAIN_INDEX}.der.parsed | grep -A 4 'X509v3 CRL Distribution Points' | grep "URI" | sed -e "s/URI://" -e "s/^[[:space:]]*//" -e "s/[[:space:]]*$//")"
CRL_PEM=${TMP_PREFIX}_crl_${CHAIN_INDEX}.pem
if [ -n "${CRL_URI}" ]; then
    logNotice "Downloading ${CRL_URI}"
    curl -m ${CRL_DOWNLOAD_TIMEOUT} -o ${TMP_PREFIX}_crl_${CHAIN_INDEX}.crl ${CURL_OPTS} "${CRL_URI}" 2>/dev/null || check_crl_failed "Failed to download" 3
    # try to convert to PEM; if failed, it is already in PEM format
    if ! openssl crl -inform DER -in ${TMP_PREFIX}_crl_${CHAIN_INDEX}.crl -outform PEM -out ${CRL_PEM} >/dev/null 2>&1; then
        # if failed, it is already in PEM format
        logNotice "CRL file is already in PEM format"
        mv ${TMP_PREFIX}_crl_${CHAIN_INDEX}.crl ${CRL_PEM}
    fi
else
    # no CRL, consider as verified
    logNotice "No CRL URI"
    check_crl_verified
fi

CA_FILE_OPT=
if [ ${CHAIN_INDEX} -gt 1 ]; then
    UPPER_CHAIN_INDEX=$((CHAIN_INDEX - 1))
    rm -f ${TMP_PREFIX}_ca_file_${CHAIN_INDEX}.pem
    touch ${TMP_PREFIX}_ca_file_${CHAIN_INDEX}.pem
    for index in $(seq ${UPPER_CHAIN_INDEX}); do
        cat ${TMP_PREFIX}_cert_${index}.pem >> ${TMP_PREFIX}_ca_file_${CHAIN_INDEX}.pem
    done
    CA_FILE_OPT="-CAfile ${TMP_PREFIX}_ca_file_${CHAIN_INDEX}.pem"
fi

if openssl verify -CApath "${CA_PATH}" ${CA_FILE_OPT} -verbose -crl_check -CRLfile ${CRL_PEM} ${TMP_PREFIX}_cert_${CHAIN_INDEX}.pem > ${TMP_PREFIX}_crl_result_${CHAIN_INDEX} 2>&1; then
    check_crl_verified
else
    # for simplicity, consider all error cases as revoked certificate
    check_crl_failed "Main verification step failed" 23
fi

exit 254
