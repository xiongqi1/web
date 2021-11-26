#!/bin/sh

###############################################################################
#
# send_test_email.cgi
#
# Parses html request post data for email settings and uses those to send out
# a test email. Returns result in an html response with a cgiresult value
# of 1 being success and any other value as a failure.
#
##############################################################################

if [ -z  "${SESSION_ID}" -o "${SESSION_ID}" != "${sessionid}" ]; then
    exit 0
fi

# CSRF token must be valid
if [ "$csrfToken" = "" -o "$csrfTokenGet" = "" -o "$csrfToken" != "$csrfTokenGet" ]; then
    exit 254
fi

. /etc/variant.sh

recipient=
server=
port=
username=
password=

#
# Parses the post data from the html request. The post data needs to
# contain the recipient email, email server address, email server
# port, user name and user password.
#
# An example of the post data string:
#
#    recipient=test%40netcommwireless.com&server=smtp.gmail.com&port=25&username=user%40gmail.com&password=blahblah
#
# As shown in the example the post data can contain %HH encoded characters.
# These are converted back to the ASCII equivalent.
#
parse_post_data()
{
    # Read the post data and split out individual key value pairs.
    local post_data=
    read -n ${CONTENT_LENGTH} post_data
    local split_data="`echo ${post_data} | tr "&" "\n"`"

    # Store each value into a variable that has the same name as the key
    for data in ${split_data}
    do
        local key="${data%=*}"
        local val="${data#*=}"
        case "${key}" in
            recipient|server|port|username|password|security|useauth)
                export "${key}"="$(convert_percentage_chars "${val}")"
                ;;
            *)
                logger "send_test_email: Invalid post data ${data}"
                ;;
        esac
    done
}

#
# Converts the hex percentage chars in a URL encoded string into the
# corresponding ASCII characters. For example, "alan.au%40netcommwireless.com"
# would be converted to "alan.au@netcommwireless.com"
#
# Args:
#    $1 = The URL encoded string
# Returns:
#    The URL encoded string with the percentage hex values converted to ASCII
#    characters.
#
convert_percentage_chars()
{
    echo "$1" | awk '
    #
    # Converts a string representation of a two digit hex number into
    # its numeric equivalent
    #
    function hex_str_to_num(hh) {
        hh_lower = tolower(hh)
        h1 = substr(hh_lower, 1, 1)
        h0 = substr(hh_lower, 2, 1)
        hexstr = "123456789abcdef"
        return index(hexstr, h1) * 16 + index(hexstr, h0)
    }

    {
        str = $0;
        while(match(str, /%../)) {
            if (RSTART > 1) {
               # emit everything before the %
               printf "%s", substr(str, 1, RSTART - 1)
            }

            # emit the two digit hex as an ascii char
            printf "%c", hex_str_to_num(substr(str, RSTART + 1, 2));

            # move past the parsed part of the string
            str = substr(str, RSTART + RLENGTH);
        }

        # emit the remaining string
        print str
    }'
}

#
# main
#

parse_post_data

BRAND_NAME="NetComm Wireless"
test "$V_SKIN" = "VDF" && BRAND_NAME="Vodafone MachineLink"

# Sanity check the post data values and send out the test email
if [ -z "${server}" -o -z "${port}"  -o -z "${recipient}" -o -z "${security}" ]; then
    logger -t user.notice "send_test_email: Missing parameters"
    cgiresult=0
else
    email_text="
*** This is an automatically generated email, please do not reply ***

Hello,

Please note that this is a test email.

Regards,

Your ${BRAND_NAME} Mobile Broadband Router
"
email_type="Email server settings test."
    if [ "${useauth}" = "0" ]; then
	# No auth at all
        send_email.sh -s "${security}" -a "${server}" -p "${port}" -x "${recipient}" "${email_text}" "${email_type}"
    elif [ "${password}" = "**********" ]; then
	# Auth, but the user hasn't changed the password - use the one from RDB
        send_email.sh -s "${security}" -a "${server}" -p "${port}" -u "${username}" "${recipient}" "${email_text}" "${email_type}"
    else
	# Auth with password
        send_email.sh -s "${security}" -a "${server}" -p "${port}" -u "${username}" -w "${password}" "${recipient}" "${email_text}" "${email_type}"
    fi
    cgiresult=$?
fi

# Return the test email send result as an HTML POST response
cat << EOF
Status: 200
Content-type: application/json
Cache-Control: no-cache
Connection: keep-alive

{
    "cgiresult": $cgiresult
}
EOF
