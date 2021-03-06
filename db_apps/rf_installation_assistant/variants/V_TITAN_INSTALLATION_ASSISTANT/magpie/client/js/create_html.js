//
// This file contains support javascript functions used by
// Titan installation assistant. Functions in this file generate
// HTML for dynamic tables
//

th_class_prefix="<th class='align12-2'>";

// A helper to convert data in bytes/second as generated by server side
// FTP measuring script to display-able string in Mbits/sec
function getThroughPutString(server_speed_data) {
    // server gives bytes per second, convert to Mbits/sec
    // Note that 1 Megabit (unlike Megabyte) is 125000 bytes
    if (isFinite(server_speed_data)) {
        var throughput = (Number(server_speed_data))/125000;
        return (throughput.toFixed(2) + " Mbps"); // AT&T wants only 2 decimal places
    } else {
        // just use the string as is from the server (could be N/A or whatever)
        return server_speed_data;
    }
}

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

// has the user entered the cell in data entry screen (return true if so).
function isUserSelectedCell(cid) {
    if (cid) {
        for (var i = 0 ; i < 3 ; i++) {
            if ((g_cid[i] !== null) && (g_cid[i] == cid)) {
                return true;
            }
        }
    }
    return false;
}

// Page 2 top part (current RF scan)
// Generate table with current RF data
// data is what comes from the server in web socket
// full_mode true means normal RF scan page, if false this is the advanced scan/status page
function buildScanResultTable(data, full_mode) {
    var scanResultTable = "<table>";
    var allowTTest = false;
    rsrp_unit_str = " (" + data.limits.RSRP.unit + ") ";
    rsrq_unit_str = " (" + data.limits.RSRQ.unit + ") ";
    scanResultTable += "<tr>\n";
    scanResultTable += th_class_prefix+"Cell sector ID</th>\n";
    scanResultTable += th_class_prefix+"Heading</th>\n";
    scanResultTable += th_class_prefix+"Serving Cell</th>\n";
    if (full_mode) {
        scanResultTable += th_class_prefix+"Minimum RSRP</th>\n"; // although this is actually RSRP Pass level
    }
    scanResultTable += th_class_prefix+"Measure</th>\n";
    scanResultTable += th_class_prefix+"Value</th>\n";
    if (full_mode) {
        scanResultTable += th_class_prefix+"Result</th>\n";
    }
    scanResultTable += "</tr>\n";
    for (var i = 0; i < data.current_readings.length; i++) {

        // detection of serving cell
        var is_serving_cell = ((data.current_readings[i].cell_sector_id > 0) && (data.current_readings[i].cell_sector_id == data.current_readings[i].serving_cell));
        if (is_serving_cell) {
            row_span_str = "<td rowspan=3>"; // will be adding rssinr
        } else {
            row_span_str = "<td rowspan=2>";
        }
        scanResultTable += "<tr>\n";
        scanResultTable += row_span_str;
        if (data.current_readings[i].cell_sector_id) {
            scanResultTable += data.current_readings[i].cell_sector_id;
        } else {
            scanResultTable += data.current_readings[i].pci;
            scanResultTable += " (PCI)";
        }
        scanResultTable += "</td>\n";

        // heading
        scanResultTable += row_span_str;
        if (data.current_readings[i].hasOwnProperty("heading")) {
            scanResultTable += data.current_readings[i].heading + "&deg;";
        }
        scanResultTable += "</td>";

        if (is_serving_cell) {
            scanResultTable += row_span_str + "*</td>"; // asterisk for serving cell
        }
        else {
            scanResultTable += row_span_str + " </td>"; // no asterisk otherwise
        }

        if (full_mode) { // no need to display min RSRP in advanced mode
            scanResultTable += row_span_str;
            // rsrp_pass is per PCI as user can select it, so we cannot use the global
            //scanResultTable += data.limits.RSRP.pass + rsrp_unit_str;
            scanResultTable += data.current_readings[i].rsrp_pass + " " + rsrp_unit_str;
            scanResultTable += "</td>\n";
        }
        scanResultTable += "<td>RSRP " + rsrp_unit_str + "</td>\n";
        scanResultTable += "<td";
        // only colour in full mode.
        if (full_mode) {
            if (data.current_readings[i].rsrp < data.current_readings[i].rsrp_pass) {
                scanResultTable += " class=\"red-font\"";
            } else {
                scanResultTable += " class=\"green-font\"";
            }
        }

        scanResultTable += ">";
        scanResultTable += data.current_readings[i].rsrp;
        scanResultTable += "</td>\n";

        if (full_mode) {
            scanResultTable += row_span_str;
            scanResultTable += data.current_readings[i].res;

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
                // an interesting effect of setting the low and high to the same as "value" is
                // that the bar becomes yellow. So make it never the same.
                scanResultTable += " low=";
                scanResultTable += (data.current_readings[i].rsrp_pass - 0.0001);
                scanResultTable += " high=";
                scanResultTable += (data.current_readings[i].rsrp_pass - 0.0001);
                scanResultTable += " optimum=";
                scanResultTable += data.limits.RSRP.max;
                scanResultTable += "></meter></p>";
            }
            scanResultTable += "</td>\n";
        }
        scanResultTable += "</tr>\n";
        // add RSSINR for serving cell only
        if (is_serving_cell) {
            scanResultTable += "<td>RSSINR " + rsrq_unit_str + "</td>\n"; // can re-use rsrq units - (dB)
            scanResultTable += "<td>" + data.current_readings[i].rssinr + "</td>\n";
            scanResultTable += "</tr>\n";
        }
        scanResultTable += "<td>RSRQ " + rsrq_unit_str + "</td>\n";
        scanResultTable += "<td>" + data.current_readings[i].rsrq + "</td>\n";
        scanResultTable += "</tr>\n";
    }
    scanResultTable += "</table>";
    return [scanResultTable, data.data_connection_unavailable];
}

// Page 2 - Stats section
// Generate table with RF stats data
function buildStatsResultTable(data) {
    var statsTable = "<table>\n";

    statsTable += "<tr>\n";
    statsTable += th_class_prefix+"Cell sector ID</th>\n";
    statsTable += th_class_prefix+"Samples</th>\n";
    statsTable += "<th class='align12-2-center' colspan=3>RSRP</th>\n";
    statsTable += "<th class='align12-2-center' colspan=3>RSRQ</th>\n";
#ifdef V_ORIENTATION_y
    statsTable += "<th class='align12-2-center' colspan=3>Best Orientation</th>\n";
#endif
    statsTable += "</tr>\n";

    // the secondary (min/max) header
    statsTable += "<tr>\n";
    statsTable += "<td colspan=2></td>\n";
    statsTable += "<td>MIN</td>\n";
    statsTable += "<td>MAX</td>\n";
    statsTable += "<td>AVG</td>\n";
    statsTable += "<td>MIN</td>\n";
    statsTable += "<td>MAX</td>\n";
    statsTable += "<td>AVG</td>\n";
#ifdef V_ORIENTATION_y
    statsTable += "<td>Azimuth</td>\n";
    statsTable += "<td>Elevation</td>\n";
    statsTable += "<td>Accuracy</td>\n";
#endif
    statsTable += "</tr>\n";

    for (var i = 0; i < data.stats.length; i++) {
        statsTable += "<tr>\n";
        statsTable += "<td>";
        if (data.stats[i].cell_sector_id) {
            statsTable += data.stats[i].cell_sector_id;
        } else {
            statsTable += data.stats[i].pci;
            statsTable += " (PCI)";
        }
        statsTable += "</td>\n";
        statsTable += "<td>";
        statsTable += data.stats[i].ns;
        statsTable += "</td>\n";
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
        statsTable += data.stats[i].rsrq_min;
        statsTable += "</td>\n";
        statsTable += "<td>";
        statsTable += data.stats[i].rsrq_max;
        statsTable += "</td>\n";
        statsTable += "<td>";
        statsTable += data.stats[i].rsrq_avg;
        statsTable += "</td>\n";
#ifdef V_ORIENTATION_y
        if (data.stats[i].orientation == null) {
            statsTable += "<td>N/A</td>";
            statsTable += "<td>N/A</td>";
            statsTable += "<td>N/A</td>";
        } else {
            statsTable += "<td>";
            statsTable += data.stats[i].orientation.azimuth
            statsTable += " &deg;</td>\n";
            statsTable += "<td>";
            statsTable += data.stats[i].orientation.elevation
            statsTable += " &deg;</td>\n";
            statsTable += "<td>";
            statsTable += data.stats[i].orientation.accuracy
            statsTable += "</td>\n";
        }
#endif
        statsTable += "</tr>\n";
    }
    statsTable += "</table>";
    return statsTable;
}

// Chose correct font colour based on speed for speed text
function colourSpeedText(server_speed_data, speed_expected) {
    if (isFinite(server_speed_data) && isFinite(speed_expected)) {
        if (Number(server_speed_data) >= Number(speed_expected)) {
            return " class=\"green-font\"";
        } else {
            return " class=\"red-font\"";
        }
    } else {
        return "";
    }
}

// Work out the overall result after all tests are completed
// assume it is a pass if no more than one test repeat has failed for download and upload,
// and the average speed is ok for both.
function getOverallResult(test_data, warn_code, is_html) {

    // must match g_warning_codes in data_collector.lua
    var warn_text = [
        "", // no worries
        "Serving cell changed during the test",
        "Serving cell not entered by the operator",
        "Serving cell is not the first choice cell entered"
        ];

    var base_str = "Overall result: ";
    var res_str = ["PASS", "FAIL"];
    var html_prefix = ["<b style=\"color:green\">", "<b style=\"color:red\">", "<b style=\"color:orange\">"];
    var s = ""; // return an empty string by default
    var overallPassed = false;
    if ((test_data.length == 2) &&
        (test_data[0].repeat_no == test_data[0].repeats) &&
        (test_data[1].repeat_no == test_data[1].repeats) &&
        (test_data[0].status == "completed") &&
        (test_data[1].status == "completed")) {
            if ((test_data[0].succ_count >= test_data[0].repeats - 1) &&
                (test_data[1].succ_count >= test_data[1].repeats - 1)) {
                overallPassed = true;
                s = base_str + res_str[0] + "\n";
                if (is_html) {
                    s = html_prefix[0] + s + "</b>";
                }
            } else {
                s = base_str + res_str[1] + "\n";
                if (is_html) {
                    s = html_prefix[1] + s + "</b>";
                }
            }
            if (warn_code != 0) {
                if (is_html) {
                    s += "<br>" + html_prefix[2] + warn_text[warn_code] + "</b>";
                } else {
                    s += warn_text[warn_code];
                }
            }
    }
    return {overallHtml: s, overallPassed: overallPassed};
}

// Page 3 - throughput test
// Generate an HTML table with throughput test data on throughput test page
function buildThroughputTestResult(data) {

    var tTestResult = "<table>\n";
    tTestResult += "<tr>\n";
    tTestResult += th_class_prefix+"Test</th>\n";
    tTestResult += th_class_prefix+"Status</th>\n";
    tTestResult += th_class_prefix+"Progress</th>\n";
    tTestResult += th_class_prefix+"Speed</th>\n";
    tTestResult += "</tr>\n";

    for (var i = 0; i < data.test_data.length; i++) {
        tTestResult += "<tr>\n";
        tTestResult += "<td>";
        tTestResult += data.test_data[i].type;
        tTestResult += "</td>\n";

        tTestResult += "<td>";
        tTestResult += data.test_data[i].status;
        if (data.test_data[i].status != "not started") {
            tTestResult += " (" + data.test_data[i].repeat_no + " of " + data.test_data[i].repeats + ") ";
        }
        tTestResult += "</td>\n";
        tTestResult += "<td>";

        tTestResult += "<progress id=\"tt_progress_bar_id\" value=\"";
        tTestResult += data.test_data[i].value;
        tTestResult += "\" max=\"";
        tTestResult += data.test_data[i].max;
        tTestResult +="\" </progress>";
        tTestResult += "</td>\n";

        tTestResult += "<td" + colourSpeedText(data.test_data[i].avg_speed, data.test_data[i].speed_expected) + ">";
        tTestResult += getThroughPutString(data.test_data[i].avg_speed);
        tTestResult += "</td>\n";
        tTestResult += "</tr>\n";
    }
    tTestResult += "</table>";
    // use the first element
    if (data.test_data.length > 0) {
        tTestResult += "<p>Serving cell: " + data.gen_data.serving_cell + "</p>";
        tTestResult += "<p>Test start time: " + retrieveTimestamp("testStart") + "</p>";
        tTestResult += "<p>" + getOverallResult(data.test_data, data.gen_data.warning, true).overallHtml + "</p>";
    } else {
        tTestResult = "<p><img src=\"/loading.gif\"></p>";
    }

    return tTestResult;
}

// Special page - no valid RF data
function buildInvalidResult(data) {
    var res = "<p>" + data.message + "</p";
    return res;
}
