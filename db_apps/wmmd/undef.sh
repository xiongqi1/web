#!/bin/sh

#
# the scripts removes lines between "$str_begin" and "$str_end"
#


str_begin="$1"
str_end="$2"

shift 2

sed -rn "
	:start
	/$str_begin/ {

		/$str_end/ {
			b fini
		}

		:ignore
		n
		/$str_end/! {
			b ignore
		}

	:fini
		n
	}

	p
" "$@"
