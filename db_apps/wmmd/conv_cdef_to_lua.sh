#!/bin/sh

#
# The script converts C define into Lua table
#

sed -rn '
	/#define[[:blank:]]+QMI_[A-Z0-9_]+_V[0-9]+[[:blank:]]+0[xX]+[[:xdigit:]]+/ {

		# take content only
		s/.*#define[[:blank:]]+(QMI_[A-Z0-9_]+_V[0-9]+[[:blank:]]+0[xX]+[[:xdigit:]]+).*/\1/g

		/^QMI_CTL_/b
		/^QMI_GET_/b

		/^QMI_IMSVT_/b
		/^QMI_COEX_/b
		/^QMI_IMSRTP_/b
		/^QMI_LOWI_/b
		/^QMI_QCMAP_/b
		/^QMI_ADC_/b
		/^QMI_ATP_/b

		s/(QMI_[A-Z0-9_]+)(IND_MSG|IND)_V[0-9]+[[:blank:]]+(0[xX]+[[:xdigit:]]+)/i.\1IND=\3/p
		s/(QMI_[A-Z0-9_]+)_(REQ_MSG|RESP_MSG|REQ|RESP)_V[0-9]+[[:blank:]]+(0[xX]+[[:xdigit:]]+)/m.\1=\3/p
	}

' | sort -u
