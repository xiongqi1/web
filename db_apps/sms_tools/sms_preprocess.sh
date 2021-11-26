#!/bin/sh

sed  '
	# remove comment line
	/^[[:space:]]*#/d

	# merge all slashed lines
	:a
	/\\[[:space:]]*$/ {
		N
		s/\\[[:space:]]*\n//g
		ba
	}

	# remove blank lines
	/^[[:space:]]*$/d
' "$@"

