#!/bin/sh
make --silent
if [ $? -ne 0 ]; then
    exit $?
fi
echo
echo "TEST USAGE:"
./leds_monitor --help
echo
echo "TEST LIST:"
./leds_monitor list
echo
echo "TEST UPDATE:"
./leds_monitor --nosyslog update
echo
echo "TEST SET:"
./leds_monitor --nosyslog set system.leds.power "red@01:100/1000"
echo
./leds_monitor --nosyslog set system.leds.battery "ffa07a@10:100/1000"
echo
./leds_monitor --nosyslog set system.leds.antenna "blue@10111000:100/0"
echo
./leds_monitor --nosyslog set system.leds.signal.0 "green"
echo
./leds_monitor --nosyslog set system.leds.signal.1 "on"
echo
echo "TEST RESET:"
./leds_monitor --nosyslog reset system.leds.power
echo
./leds_monitor --nosyslog reset
echo
echo "TEST MONITOR:"
./leds_monitor --nosyslog monitor
echo

