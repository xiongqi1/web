#!/bin/sh

if [ -z "$TESTS" ]; then
	TESTS=*/*.lua
fi
if [ -n "$1" ]; then
	TESTS="$@"
fi

echo -n 'Running Tests: '

for TEST in $TESTS ; do
	echo -n " $TEST"
	lua ${TEST} > /tmp/test-$$.tmp 2>&1
	if [ $? -gt 0 ] ; then
		echo
		echo '*******************'
		echo '*** TEST FAILED ***'
		echo '*******************'
		echo
		echo $TEST
		echo
		echo '--8><---------------------------------------------------------------------------'
		cat /tmp/test-$$.tmp
		echo '---------------------------------------------------------------------------><8--'
		exit 1
	else
		echo -n '.'
	fi
	rm -r /tmp/test-$$.tmp
done

echo ' Completed.'
