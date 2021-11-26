#ifndef _BATTERY_H
#define _BATTERY_H

/* Call once at start */
int init_battery(int a, int b);

/* Get state */
int battery_charging(void);
int battery_usb(void);
int battery_voltage(void);

/*
Call this on updates. Returns a filtered battery voltage
in mV. Also sets database variables and charge/discharge
LED state.

Database variables:

battery.voltage = 4-digit number, voltage in mV, typically 3500-4400
battery.status  = single character, c=charging, d=discharging
battery.percent = integer battery full percentage.

*/
int poll_battery(int supply_present, int charging, int voltage);

#endif /* _BATTERY_H */
