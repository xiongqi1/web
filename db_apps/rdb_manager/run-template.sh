#!/bin/sh

#
# This script runs a template only for test purpose. Do not use this script in production
#
# Yong

template=$1

print_usage() {
	cat << EOF
This script is only for testing purposes. It executes a single template, specified as the first argument. Do not use this script in production.
Usage:	
	run-template.sh <one.template>
EOF
}

# check validation
if [ $# = 0 ]; then
	echo "missing parameter" >&2
	print_usage >&2
	exit 0
fi

if [ "$1" = "-h" -o "$1" = "--help" ]; then
	print_usage >&2
	exit 0
fi

# check file
if [ ! -x "$template" ]; then
	echo "file not found or permission error - $1" >&2
	print_usage >&2
	exit 0 
fi

template_base_name=$(basename "$template")

# replace escape sequence
echo "copying the template to /tmp/$template_base_name"
cat "$1" | sed 's/\?<\(.*\)>\;/$(rdb_get "\1")/g' > "/tmp/$template_base_name"

# run the template
echo "lauching /tmp/$template_base_name"
chmod 755 "/tmp/$template_base_name"
"/tmp/$template_base_name"
