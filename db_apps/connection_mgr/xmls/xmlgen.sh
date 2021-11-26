#!/bin/sh

# check platform
if [ -z "$PLATFORM" ]; then
	echo "PLATFORM not specified" >&2
	exit 1
fi

# check board
if [ -z "$V_BOARD" ]; then
	echo "V_BOARD not specified" >&2
	exit 1
fi

# check skin
if [ -z "$V_SKIN" ]; then
	echo "V_SKIN not specified" >&2
	exit 1
fi

PLATFORM=$(echo $PLATFORM | tr '[:upper:]' '[:lower:]')
V_BOARD=$(echo $V_BOARD | tr '[:upper:]' '[:lower:]')
V_SKIN=$(echo $V_SKIN | tr '[:upper:]' '[:lower:]')

ls_egrep() {
	for r in $@; do
		ls | egrep "$r"
	done
}

# get addon xmls
ADDONS=$(ls_egrep "addon-[0-9]+.xml$" "addon-${PLATFORM}-[0-9]+.xml$" "addon-${PLATFORM}-${V_BOARD}-[0-9]*.xml$" "addon-${PLATFORM}-${V_BOARD}-${V_SKIN}-[0-9]+.xml$" | tr '\n' ' ')

echo "xmlgen: platform=${PLATFORM}, board=${V_BOARD}, skin=${V_SKIN}"

if [ -n "$ADDONS" ]; then
	echo "xmlgen: applying xml addons... ($ADDONS)"
	./merge_apn -b base.xml $ADDONS | ./xmlindent.sh > output.xml 
else
	echo "xmlgen: no addon found"
	cp base.xml output.xml
fi
