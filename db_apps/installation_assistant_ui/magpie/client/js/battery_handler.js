/*
Copyright (C) 2019 Casa Systems.

Class processing battery data
*/

/*
 * Constructor
 */
function BatteryHandler() {
    this.percentage = -1;
}

/*
 * Handle battery data
 * @param data Battery data
 */
BatteryHandler.prototype.handleData = function(data) {
    this.percentage = data.charge_percentage;
}
