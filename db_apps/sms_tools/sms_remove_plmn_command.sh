#!/bin/sh

sed  '
	# remove all lines including plmn
	/plmn/d
' "$@"

