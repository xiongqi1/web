//
// This file contains support javascript functions used by
// Titan installation assistant. Functions in this file generate
// HTML for dynamic tables
//

th_class_prefix="<th class='align12-2'>";


#ifdef V_TITAN_INSTALLATION_ASSISTANT_CONN_nrb200
// Shown in Page 1 - battery section if it's available
// Generate HTML5 element (meter or progress) with battery data
function buildBatteryResult(data) {
    var battResult;

    if (!g_is_html5_supported) {
        battResult = "<p><progress value=";
        battResult += data.charge_percentage;
        battResult += " max=100></progress>";
    } else {
        battResult = "<p><meter value=";
        battResult += data.charge_percentage;
        battResult += " min=0 max=100 low=40 high=79 optimum=100></meter>";
    }
    battResult += " " + data.charge_percentage + "%" + "</p>";
    battResult += "<p>" + data.status + "</p>";
    return battResult;
}
#endif

#ifdef V_ORIENTATION_y
// Shown in Page 2 - orientation section if it's available
function buildMagCalStatus(data) {
    var magCalStatus = "<table style=width:100%;margin:0 0 0 0;>";
    magCalStatus += "<tr>";
    magCalStatus += "<td style=padding-left:20px>Magnetometer Status: </td>";
    if (data.mag_cal_status) {
        magCalStatus += "<td>" + data.mag_cal_status + "</td>";
    } else {
        magCalStatus += "<td>Not Calibrated</td>";
        data.mag_cal_status = "Not Calibrated"
    }
    magCalStatus += "<td><button "
    if (data.mag_cal_status == "Not Calibrated" || data.mag_cal_status == "Calibration Failed") {
        magCalStatus += "style=background:red ";
    } else if (data.mag_cal_status == "Calibration In Progress") {
        magCalStatus += "disabled ";
    }
    magCalStatus += "class='tinyfont' id='StartCalibrationButtonId' onclick='startMagnetometerClibration()'>Start</button></td>"
    magCalStatus += "</tr>";
    magCalStatus += "</table>";
    return magCalStatus
}
#endif


// Constants supporting the drawing of carets
// Bar extents and ranges for working out colour
var MIN_RSRP = -120
var MIN_RSRP_TH = -118
var MAX_RSRP = -60

// caret, which is a semi-persistent range of values
var g_lowest_caret = 0;
var g_highest_caret = 0;
// how quickly caret converges on the current value
var CARET_SPEED = 5
var g_caret_iter = CARET_SPEED

function get_carets(value) {
    var item = []

    if (value > MAX_RSRP) {
        value = MAX_RSRP
    } else if (value < MIN_RSRP) {
        value = MIN_RSRP
    }

    // set caret first time - to current reading
    if (g_lowest_caret == 0) {
        g_lowest_caret = value
    }

    if (g_highest_caret == 0) {
        g_highest_caret = value
    }

    // caret range checking
    if (value > g_highest_caret) {
        g_highest_caret = value
    }

    if (value < g_lowest_caret) {
        g_lowest_caret = value
    }

    // requirement is to show semi-persistent averages.
    // Use some simple rolling average math to make the caret follow converging average value
    // MUCH better than wasting memory and keeping all samples.
    // The end result looks very much like Mode1
    g_caret_iter = g_caret_iter - 1
    if (g_caret_iter == 0) {
        g_caret_iter = CARET_SPEED
        g_lowest_caret = (3 * g_lowest_caret + value) / 4 // new_avg = (3*old_avg + sample)/4
        g_highest_caret = (3 * g_highest_caret + value) / 4
    }

    item[0] = Math.round(g_lowest_caret)
    item[1] = Math.round(g_highest_caret)

    return item
}

function buildStatusCombined(value) {
    var info = "<mark class=\"";
    if (value == 'Disabled') {
        info += "bgcblack\">D";
    } else if (value == 'NoLink') {
        info += "bgcyellow\"><font color=\"black\">N</font>";
    } else if (value == 'Up') {
        info += "bgcgreen\">U";
    } else {
        // value == 'Error' or any invalid value
        info += "bgcred\">E";
    }
    info += "</mark>";
    return info
}

// return the bar size (also works for caret location)
// go from 0 to 24, mapped to 60 db,
// bar size is from 0 to 24
function get_bar_size(rsrp) {
    var bar_size = 0
    if (rsrp > MAX_RSRP) {
        bar_size = 60;
    } else if (rsrp > MIN_RSRP_TH) {
        bar_size = parseInt(rsrp, 10) + 120;
    } else if (rsrp > MIN_RSRP) {
        bar_size = 3;
    }
    bar_size = parseInt((bar_size / 2.5), 10);
    if (bar_size > 24) {
        bar_size = 24;
    } else if (bar_size < 0) {
        bar_size = 0;
    }
    return bar_size
}

function getRsrpColor(rsrp) {
    if (rsrp < -99) {
        return "red";
    } else if (rsrp < -96) {
        return "yellow";
    } else {
        return "green";
    }
}

function getcr(w, t) {
    return getcr2(w, "\">" + t)
}

function getcr2(w, t) {
    return "<td colspan=\"" + w + "\" style=\"text-align:right;" + t + "</td>"
}

function getcl(w, t) {
    return "<td colspan=\"" + w + "\" style=\"text-align:left;\">" + t + "</td>"
}

function getcl2(w, t) {
    return "<td colspan=\"" + w + "\" style=\"text-align:left;" + t + "</td>"
}

function getpre(t) {
    return "<pre>" + t + "</pre>"
}

// Helper to map RDB value (as per indoor.led_ RDBs
// 0 off, 1 red, 2 green, 3 amber, 4 off, 5 red flashing, 6 green flashing, 7 red flashing
// returns two parameters, colour and blink status (true/false)
function value_to_color(value) {
    var blink = " "
    var led_color_table= [
        "black", "red", "green", "yellow; color:black",
        "black", "red", "green", "yellow; color:black"]

    if (value >= 4) {
        blink = "B"
    }
    return [led_color_table[value], blink]
}


function get_led(value) {
    return getcr(1, "<pre style=\"background-color: " + value[0] + ";\">" + value[1] + "</pre>");
}


function buildMode2Info(data) {
    var mode2Info = "<table>";
    var spc = "<td colspan=\"1\"><pre> </pre></td>";
    var carets = [MIN_RSRP, MIN_RSRP]

    if (data.attached == 0) {
        data.p_pci = "";
        data.p_earfcn = "";
        data.p_rsrp = "";
        data.p_rsrp0 = "";
        data.p_rsrp1 = "";
        data.p_sinr = "";
        data.state = "";
        data.p_band = "";
        data.p_tx_power = "";
        data.avc_rx = "";
        data.avc_tx = "";
    }

    var ncell = []
    for (var i=0; i<4; i++) {
        if (!data.ncell[i]) {
            data.ncell[i] = {pci:"",arfcn:"",rsrp:""}
        }
        ncell[i] = getcr(5, data.ncell[i].arfcn);
        ncell[i] += spc;
        ncell[i] += getcr(3, data.ncell[i].pci);
        ncell[i] += spc;
        ncell[i] += getcr(6, data.ncell[i].rsrp);
    }

    var alarms = []
    for (var i=0; i<4; i++) {
        alarms[i] = getcl2(45, " letter-spacing:1.3px;\">" + (data.alarms[i] || ""));
    }

    // heading row
    mode2Info += "<tr>";
    // required for allignment
    for (var j=0; j<80; j++) {
        mode2Info += getcl(1,  "<pre style=\"line-height:0px\"> </pre>");
    }

    // row 1
    mode2Info += "<tr>"
    mode2Info += getcl(19, getpre("MODE 2 DIAGNOSTICS "));
    var sw_ver = data.sw_ver.substring(0,4)
    if (sw_ver == "NBNe" ) {
        sw_ver = data.sw_ver.substring(7, 18)
    } else {
        sw_ver = data.sw_ver.substring(4, 15)
    }
    mode2Info += getcr(11, sw_ver);

    mode2Info += getcr(1, getpre("/"));
    mode2Info += getcr(5, data.hw_ver);

    mode2Info += spc;
    mode2Info += getcr(19, data.date_time);
    mode2Info += getcr(2, getpre(" "));
    // 8 digits cover 3 years, 9 digits cover 31 years of uptime
    mode2Info += getcr(9, data.uptime);
    mode2Info += getcr(2, getpre(" "));
    mode2Info += getcr(12, data.serial);

    // row 2
    mode2Info += "<tr>" + getcr(80, getpre(" "));

    // row 3
    mode2Info += "<tr>";
    mode2Info += getcl(6, getpre(" "));
    mode2Info += getcl(5, getpre("ARFCN"));
    mode2Info += spc;
    mode2Info += getcl(3, getpre("PCI"));
    mode2Info += spc;
    mode2Info += getcl(6, getpre("RSRP"));
    mode2Info += spc;
    mode2Info += getcl(4, getpre("SINR"));
    mode2Info += getcl(7, getpre(" "));
    mode2Info += getcl(24, getpre("RSRP(dBm)"));
    mode2Info += getcr(22, getpre("SINR  %"));

    // row 4
    mode2Info += "<tr>";
    mode2Info += getcl(6, getpre("Conn"));
    mode2Info += getcr(5, data.p_earfcn);
    mode2Info += spc;
    mode2Info += getcr(3, data.p_pci);
    mode2Info += spc;
    mode2Info += getcr(6, data.p_rsrp0);
    mode2Info += spc;
    mode2Info += getcr(4, data.p_sinr);
    mode2Info += getcr(7, getpre("-120|"));

    if (data.attached != 0) {
        var bar_size = 0;
        var bk_color = getRsrpColor(data.p_rsrp)

        bar_size = get_bar_size(data.p_rsrp);
        if (bar_size > 0) {
            mode2Info += getcl(bar_size, "<pre style=\"background-color:" + bk_color + "\"> </pre>");
        }
        if (bar_size < 24) {
            mode2Info += getcl((24 - bar_size), "");
        }
    } else {
        mode2Info += getcl(24, "No RF Connection");
    }
    mode2Info += getcl(7, getpre("|-60"));
    mode2Info += getcr(12, getpre(">23 "));
    mode2Info += getcr2(3, "color:lightgreen;\">" + data.cinr[0]);

    // row 5 (carets)
    mode2Info += "<tr>";
    mode2Info += getcr(16, getpre(" "));
    mode2Info += getcr(6, data.p_rsrp1);
    mode2Info += getcr(12, getpre(" "));
    if (data.attached != 0) {
        carets = get_carets(parseInt(data.p_rsrp, 10))
        var lcaret = get_bar_size(carets[0]);
        var rcaret = get_bar_size(carets[1]);
        var spaces = 24
        if (lcaret == 0) {
            spaces -= 1
            if (rcaret == 24) {
                rcaret -= 1
            }
        }
        mode2Info += getcr2(lcaret, "font-weight:bold;color:red\">^");
        spaces -= lcaret
        if (lcaret != rcaret) {
            mode2Info += getcr2((rcaret - lcaret), "font-weight:bold;color:red\">^");
            spaces -= (rcaret - lcaret)
        }
        if (spaces > 0) {
            mode2Info += getcr(spaces, "");
        }
    } else {
        mode2Info += getcr(24, "");
    }
    mode2Info += spc;
    mode2Info += getcr(11, getpre("RSRP   %"));
    mode2Info += getcr(7, getpre("<23 "));
    mode2Info += getcr2(3, "color:lightgreen;\">" + data.cinr[1]);

    // row 6
    mode2Info += "<tr>";
    mode2Info += getcr(55, getpre("12345678"));
    mode2Info += getcr(12, getpre(">-96 "));
    mode2Info += getcr2(3, "color:lightgreen;\">" + data.rsrp[0]);
    mode2Info += getcr(7, getpre("<18 "));
    mode2Info += getcr2(3, "color:yellow\">" + data.cinr[2]);

    // row 7
    mode2Info += "<tr>";
    mode2Info += getcl(6, getpre("Neig1 "));
    mode2Info += ncell[0]
    mode2Info += getcl(7, getpre(" TXPWR "));
    mode2Info += getcr(5, data.p_tx_power);
    mode2Info += getcl(13, getpre(" UNID STATUS "));
    mode2Info += getcl(4,
        buildStatusCombined(data.unid1_status) +
        buildStatusCombined(data.unid2_status) +
        buildStatusCombined(data.unid3_status) +
        buildStatusCombined(data.unid4_status));
    mode2Info += getcr(16, getpre("<-96 "));
    mode2Info += getcr2(3, "color:yellow;\">" + data.rsrp[1]);
    mode2Info += getcr(7, getpre("<13 "));
    mode2Info += getcr2(3, "color:red;\">" + data.cinr[3]);

    // row 8
    var avc_status = ""
    mode2Info += "<tr>";
    mode2Info += getcr(6, getpre("    2 "));
    mode2Info += ncell[1]
    mode2Info += getcl(6, getpre(" STATE"));
    mode2Info += getcr(6, data.state);
    mode2Info += getcl(13, getpre(" AVC STATUS "));
    for (var i=0; i < data.avc.length; i++) {
        avc_status += buildStatusCombined(data.avc[i])
    }
    for (var i=data.avc.length; i < 8; i++) {
        avc_status += buildStatusCombined('Disabled')
    }
    mode2Info += getcl(8, avc_status)
    mode2Info += getcr(12, getpre("<-99 "));
    mode2Info += getcr2(3, "color:red;\">" + data.rsrp[2]);
    mode2Info += getcr(7, getpre("< 8 "));
    mode2Info += getcr2(3, "color:red;\">" + data.cinr[4]);

    // row 9
    mode2Info += "<tr>";
    mode2Info += getcr(6, getpre("    3 "));
    mode2Info += ncell[2]
    mode2Info += getcl(9, getpre(" PROV PCI"));
    mode2Info += getcr(3, data.pci);
    mode2Info += getcl(46, getpre(" Active Alarms:"));

    // row 10
    mode2Info += "<tr>";
    mode2Info += getcr(6, getpre("    4 "));
    mode2Info += ncell[3]
    mode2Info += getcr(9, getpre(" BAND"));
    mode2Info += getcr(3, data.p_band);
    mode2Info += spc + alarms[0];

    // row 11
    mode2Info += "<tr>" + getcr(23, getpre("BYTES RECD "));
    mode2Info += getcr(11, data.avc_rx);
    mode2Info += spc + alarms[1];

    // row 12
    mode2Info += "<tr>" + getcr(23, getpre("BYTES SENT "));
    mode2Info += getcr(11, data.avc_tx);
    mode2Info += spc + alarms[2];

    // row 13
    var status_led = ["yellow; color:black", "B"];
    var odu_led = ["black", " "];
    var s_led_l = ["black", " "];
    var s_led_m = ["black", " "];
    var s_led_h = ["black", " "];
    if (data.comms_failed == 0) {
        if (data.cable_fault == 1) {
            status_led = ["red", "B"];
        } else {
            status_led = value_to_color(data.status_led)
        }
        odu_led = value_to_color(data.odu)
        s_led_l = value_to_color(data.sig_low)
        s_led_m = value_to_color(data.sig_med)
        s_led_h = value_to_color(data.sig_high)
    }
    mode2Info += "<tr>"
    mode2Info += getcl(16, getpre("IDU LEDS STATUS "));
    mode2Info += get_led(status_led)
    mode2Info += getcr(5, getpre(" ODU "));
    mode2Info += get_led(odu_led)
    mode2Info += getcr(8, getpre(" SIGNAL "));
    mode2Info += get_led(s_led_l)
    mode2Info += get_led(s_led_m)
    mode2Info += get_led(s_led_h)
    mode2Info += spc;
    mode2Info += alarms[3];

    // row 14
    mode2Info += "<tr>" + getcl(80, getpre("Logs:"));

    // row 15 to 24 (or more if configured)
    for (var i=0; i<data.log_entries; i++) {
        mode2Info += "<tr>" + getcl2(80, " letter-spacing:1.3px;\">" + (data.log[i] || ""));
    }

    mode2Info += "</table>";
    return mode2Info
}


// Page 2 top part (current RF scan)
// Generate table with current RF data
function buildScanResultTable(data) {
    var scanResultTable = "<table>";
    var result_text = "";
    rsrp_unit_str = " (" + data.limits.RSRP.unit + ") ";
    rsrp_delta_unit_str = " (" + data.limits.RSRP_Delta.unit + ") ";
    scanResultTable += "<tr>\n";
    scanResultTable += "<th class='align12-2-center'><pre style=\"font-family:Verdana,Geneva,sans-serif;\">ECGI\nPCI   BAND   ARFCN</pre></th>";
    scanResultTable += th_class_prefix+"Result</th>\n";
    scanResultTable += th_class_prefix+"Measure</th>\n";
    scanResultTable += th_class_prefix+"Value</th>\n";
    scanResultTable += th_class_prefix+"</th>\n"; // Range low
    scanResultTable += th_class_prefix+"</th>\n"; // Reading
    scanResultTable += th_class_prefix+"</th>\n"; // Range high
    scanResultTable += "</tr>\n";
    // according to NBN requirements, sort by delta and then rsrp (best one shown first)
    data.current_readings.sort(function (a, b) {
        return b.rsrp_delta - a.rsrp_delta || b.rsrp - a.rsrp;
    end});
    for (var i = 0; i < data.current_readings.length; i++) {

        row_span_str = "<td rowspan=3>";
        scanResultTable += "<tr>\n";
        scanResultTable += row_span_str;
        scanResultTable += "<pre style=\"text-align:center;font-family:Verdana,Geneva,sans-serif;\">" + data.current_readings[i].ecgi + "\n" ;
        scanResultTable += data.current_readings[i].pci + "   " ;
        scanResultTable += data.current_readings[i].band + "   " ;
        scanResultTable += data.current_readings[i].earfcn;
        scanResultTable += "</pre></td>\n";
        scanResultTable += "<td rowspan=3";
        if ((data.current_readings[i].rsrp < data.limits.RSRP.warning) ||
            (data.current_readings[i].rsrp_delta < data.limits.RSRP_Delta.pass)) {
            scanResultTable += " class=\"red-font\"";
            result_text = "FAIL";
        } else if (data.current_readings[i].rsrp < data.limits.RSRP.pass) {
            scanResultTable += " class=\"orange-font\"";
            result_text = "CHECK";
        } else {
            scanResultTable += " class=\"green-font\"";
            result_text = "PASS";
        }
        scanResultTable += ">";
        scanResultTable += result_text;
        scanResultTable += "</td>\n";
        scanResultTable += "<td>RSRP " + rsrp_unit_str + "</td>\n";
        scanResultTable += "<td";
        // Note that RSRP can be good, but delta is bad. In this case, RSRP should still show in green
        if (data.current_readings[i].rsrp >= data.limits.RSRP.pass) {
            scanResultTable += " class=\"green-font\"";
        } else if (data.current_readings[i].rsrp >= data.limits.RSRP.warning) {
            scanResultTable += " class=\"orange-font\"";
        } else {
            scanResultTable += " class=\"red-font\"";
        }
        scanResultTable += ">";
        scanResultTable += data.current_readings[i].rsrp;
        scanResultTable += "</td>\n";
        scanResultTable += "<td style=\"text-align:right\">";
        scanResultTable += data.limits.RSRP.min + rsrp_unit_str;
        scanResultTable += "</td>\n";
        scanResultTable += "<td style=\"width:180px\">";
        //scanResultTable += data.current_readings[i].res; // no need to display Good/Bad and so on

        if (!g_is_html5_supported) {
            // progress meter needs positive values.
            scanResultTable += "<p><progress value=";
            // RSRP readings are negative. The lowest possible is -140 dBm (see data_collector).
            scanResultTable += data.current_readings[i].rsrp + 140;
            // The highest possible is -44 dBm. 140-44 is 96, so set it as max.
            scanResultTable += " max=96></progress></p>";
        } else {
            scanResultTable += "<p><meter value=";
            scanResultTable += data.current_readings[i].rsrp;
            scanResultTable += " min=";
            scanResultTable += data.limits.RSRP.min;
            scanResultTable += " max=";
            scanResultTable += data.limits.RSRP.max;
            // everything between low and high will be shown as a yellow/orange bar
            scanResultTable += " low=";
            scanResultTable += data.limits.RSRP.warning;
            scanResultTable += " high=";
            scanResultTable += data.limits.RSRP.pass;
            scanResultTable += " optimum=";
            scanResultTable += data.limits.RSRP.max;
            scanResultTable += "></meter></p>";
        }
        scanResultTable += "</td>\n";
        scanResultTable += "<td style=\"text-align:left\">";
        scanResultTable += data.limits.RSRP.max + rsrp_unit_str;
        scanResultTable += "</td>\n";

        scanResultTable += "</tr>\n";

        scanResultTable += "<td>Delta " + rsrp_delta_unit_str + "</td>\n";

        // Note that RSRP Delta can be good, but RSRP is bad. In this case, RSRP delta should still show in green
        scanResultTable += "<td";
        if (data.current_readings[i].rsrp_delta > data.limits.RSRP_Delta.pass) {
            scanResultTable += " class=\"green-font\"";
        } else {
            scanResultTable += " class=\"red-font\"";
        }
        scanResultTable += ">";

        scanResultTable += data.current_readings[i].rsrp_delta + "</td>\n";

        scanResultTable += "<td style=\"text-align:right\">";
        scanResultTable += data.limits.RSRP_Delta.min + rsrp_delta_unit_str;
        scanResultTable += "</td>\n";

        scanResultTable += "<td>";

        if (!g_is_html5_supported) {
            // progress meter needs positive values.
            scanResultTable += "<p><progress value=";
            scanResultTable += data.current_readings[i].rsrp_delta;
            // The highest possible is -44 dBm. 140-44 is 96, so set it as max.
            scanResultTable += " max=96></progress></p>";
        } else {
            scanResultTable += "<p><meter value=";
            scanResultTable += data.current_readings[i].rsrp_delta;
            scanResultTable += " min=";
            scanResultTable += data.limits.RSRP_Delta.min;
            scanResultTable += " max=";
            scanResultTable += data.limits.RSRP_Delta.max;
            // an interesting effect of setting the low and high to the same as "value" is
            // that the bar becomes yellow. So make it never the same.
            scanResultTable += " low=";
            scanResultTable += (data.limits.RSRP_Delta.pass - 0.0001);
            scanResultTable += " high=";
            scanResultTable += (data.limits.RSRP_Delta.pass - 0.0001);
            scanResultTable += " optimum=";
            scanResultTable += data.limits.RSRP_Delta.max;
            scanResultTable += "></meter></p>";
        }

        scanResultTable += "<td style=\"text-align:left\">";
        scanResultTable += data.limits.RSRP_Delta.max + rsrp_delta_unit_str;
        scanResultTable += "</td>\n";

        scanResultTable += "</tr>\n";

        scanResultTable += "<td>";
        scanResultTable += "RSRQ";
        scanResultTable += "</td>\n";

        scanResultTable += "<td>";
        scanResultTable += data.current_readings[i].rsrq;
        scanResultTable += "</td>\n";

        scanResultTable += "<td>";
        scanResultTable += "Delta PCI";
        scanResultTable += "</td>\n";

        scanResultTable += "<td>";
        scanResultTable += data.current_readings[i].pci_delta;
        scanResultTable += "</td>\n";

        scanResultTable += "</tr>\n";
    }
    scanResultTable += "</table>";
    return scanResultTable;
}

// Page 2 - Stats section
// Generate table with RF stats data
function buildStatsResultTable(data) {
    var statsTable = "<table>\n";

    statsTable += "<tr>\n";
    statsTable += "<th class='align12-2-center'><pre style=\"font-family:Verdana,Geneva,sans-serif;\">ECGI\nPCI   BAND   ARFCN</pre></th>";
    statsTable += th_class_prefix+"Samples</th>\n";
    statsTable += th_class_prefix+"Result</th>\n";
    statsTable += th_class_prefix+"Min_RSRP</th>\n";
    statsTable += th_class_prefix+"Max_RSRP</th>\n";
    statsTable += th_class_prefix+"Avg_RSRP</th>\n";
    statsTable += th_class_prefix+"Med_RSRP</th>\n";
    statsTable += th_class_prefix+"Delta_RSRP</th>\n";
    statsTable += th_class_prefix+"RSRQ</th>\n";
    statsTable += "</tr>\n";

    // according to NBN requirements, sort by delta and then rsrp (best one shown first)
    data.stats.sort(function (a, b) {
        return b.rsrp_delta_avg - a.rsrp_delta_avg || b.rsrp_avg - a.rsrp_avg;
    end});

    for (var i = 0; i < data.stats.length; i++) {
        statsTable += "<tr>\n";

        statsTable += "<td><pre style=\"text-align:center;font-family:Verdana,Geneva,sans-serif;\">" + data.stats[i].ecgi + "\n" ;
        statsTable += data.stats[i].pci + "   " ;
        statsTable += data.stats[i].band + "   " ;
        statsTable += data.stats[i].earfcn;
        statsTable += "</pre></td>\n";

        statsTable += "<td>";
        statsTable += data.stats[i].ns;
        statsTable += "</td>\n";

        // Work out what criteria to show, and in what colour.
        // 1) To be green, the RSRP has to be better than "pass" level, and delta
        // has to be good
        if ((data.stats[i].rsrp_avg >= data.limits.RSRP.pass) &&
            (data.stats[i].rsrp_delta_avg >= data.limits.RSRP_Delta.pass)) {
            statsTable += "<td class=\"green-font\">PASS</td>\n";
        } else if ((data.stats[i].rsrp_avg < data.limits.RSRP.warning) ||
            // 2) to be red, either RSRP has to be worse than warning level, or delta has to be too close
            (data.stats[i].rsrp_delta_avg < data.limits.RSRP_Delta.pass)) {
            statsTable += "<td class=\"red-font\">FAIL</td>\n";
        } else {
            // 3) and everything else is an orange CHECK.
            statsTable += "<td class=\"orange-font\">CHECK</td>\n";
        }

        statsTable += "<td>";
        statsTable += data.stats[i].rsrp_min;
        statsTable += "</td>\n";

        statsTable += "<td>";
        statsTable += data.stats[i].rsrp_max;
        statsTable += "</td>\n";

        statsTable += "<td>";
        statsTable += data.stats[i].rsrp_avg;
        statsTable += "</td>\n";

        statsTable += "<td>";
        statsTable += data.stats[i].rsrp_median;
        statsTable += "</td>\n";

        statsTable += "<td>";
        statsTable += data.stats[i].rsrp_delta_avg;
        statsTable += "</td>\n";

        statsTable += "<td>";
        statsTable += data.stats[i].rsrq_avg;
        statsTable += "</td>\n";

        statsTable += "</tr>\n";
    }
    statsTable += "</table>";
    return statsTable;
}




// Special page - no valid RF data
function buildInvalidResult(data) {
    var res = "<p>" + data.message + "</p";
    return res;
}

