#!/bin/sh
#
# detect if sys/timerfd is present
# return the name of the appropriate source file
#
cat >test.c <<CODE
#include <sys/timerfd.h>
int main(void) { return 0; }
CODE

$1 -c test.c -o test >/dev/null 2>/dev/null
RVAL=$?
if [ $RVAL -eq 0 ]; then
    echo "one_shot_timer.c"
else
    echo "one_shot_timer_basic.c"
fi

rm -f test.c
rm -f test
