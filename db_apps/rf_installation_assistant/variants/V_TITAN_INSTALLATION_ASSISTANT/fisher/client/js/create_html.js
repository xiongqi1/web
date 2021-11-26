//
// This file contains support javascript functions used by
// Titan installation assistant. Functions in this file generate
// HTML for dynamic tables
//

th_class_prefix="<th class='align12-2'>";

function displayEcgiOrEci(ecgi, sel) {
    if (ecgi === undefined || ecgi === null || ecgi.length === 0) {
        return "N/A";
    }
    if (sel) {
        return ecgi;
    }
    return ecgi.substring(6);
}

#ifdef V_ORIENTATION_y
// Shown in Page 2 - orientation section if it's available
function buildOrientationResult(data) {
    var orientationTable = "<table>\n";
    orientationTable += "<tr>\n"
    orientationTable += th_class_prefix+"Azimuth</th>\n";
    orientationTable += th_class_prefix+"Elevation</th>\n";
    orientationTable += th_class_prefix+"Accuracy</th>\n";
    orientationTable += "</tr>\n"
    orientationTable += "<tr>\n"
    if (data.azimuth) {
        orientationTable += "<td>" + displayAltStringIfEmpty(data.azimuth) + " &deg;</td>\n";
    } else {
        orientationTable += "<td>N/A</td>\n";
    }
    if (data.elevation) {
        orientationTable += "<td>" + displayAltStringIfEmpty(data.elevation) + " &deg;</td>\n";
    } else {
        orientationTable += "<td>N/A</td>\n";
    }
    if (data.accuracy) {
        orientationTable += "<td>" + displayAltStringIfEmpty(data.accuracy) + "</td>\n";
    } else {
        orientationTable += "<td>N/A</td>\n";
    }
    orientationTable += "</tr>\n"
    orientationTable += "</table>";
    return orientationTable
}
#endif

// Page 1
function buildSystemStatusContent(data) {
    var content;

    // Celluar connection status
    content = "<div class='box-header'>\n"
    content += "<h2>Cellular Connection Status</h2>\n"
    content += "</div>\n"
    content += "<div class='box-content'>\n"
    content += "<table>\n"
    content += "<tr>\n"
    content += th_class_prefix+"SIM status</th>\n";
    content += th_class_prefix+"Network Registration Status</th>\n";
    content += "</tr>\n"
    content += "<tr>\n"
    content += "<td>" + displayAltStringIfEmpty(data.CellConn.SimStatus) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.CellConn.RegStatus) + "</td>\n";
    content += "</tr>\n"
    content += "</table>\n"
    content += "</div>\n"

    // WWAN connection status
    content += "<div class='box-header'>\n"
    content += "<h2>WWAN Connection Status</h2>\n"
    content += "</div>\n"
    content += "<div class='box-content'>\n"
    content += "<table class='transposable'>\n"
    content += "<tr>\n"
    content += th_class_prefix+"APN</th>\n";
    content += th_class_prefix+"WWAN IP</th>\n";
    content += th_class_prefix+"DNS Server 1</th>\n";
    content += th_class_prefix+"DNS Server 2</th>\n";
    content += th_class_prefix+"Connection Uptime</th>\n";
    content += "</tr>\n"
    content += "<tr>\n"
    content += "<td>" + displayAltStringIfEmpty(data.WwanConn[0].APN) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.WwanConn[0].IPv4Addr) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.WwanConn[0].IPv4Dns1) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.WwanConn[0].IPv4Dns2) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.WwanConn[0].ConnUpTime) + "</td>\n";
    content += "</tr>\n"
    content += "</table>\n"
    content += "</div>\n"

    // Advanced status
    content += "<div class='box-header'>\n"
    content += "<h2>Advanced Status</h2>\n"
    content += "</div>\n"
    content += "<div class='box-content'>\n"
    content += "<table class='transposable'>\n"
    content += "<tr>\n"
    content += th_class_prefix+"SIM ICCID</th>\n";
    content += th_class_prefix+"Cell ID</th>\n";
    content += th_class_prefix+"PCI</th>\n";
    content += th_class_prefix+"EARFCN</th>\n";
    content += th_class_prefix+"RSRP(dBm)</th>\n";
    content += th_class_prefix+"RSRQ(dB)</th>\n";
    content += "</tr>\n"
    content += "<tr>\n"
    content += "<td>" + displayAltStringIfEmpty(data.Advance.SimICCID) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.Advance.CellId) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.Advance.PCI) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.Advance.EARFCN) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.Advance.RSRP) + "</td>\n";
    content += "<td>" + displayAltStringIfEmpty(data.Advance.RSRQ) + "</td>\n";
    content += "</tr>\n"
    content += "</table>\n"
    content += "</div>\n"

    // Battery status
#ifdef V_TITAN_INSTALLATION_ASSISTANT_CONN_nrb200
    if (data.Battery) {
        content += "<div class='box-header'>\n"
        content += "<h2>Battery Status</h2>\n"
        content += "</div>\n"
        content += "<div class='box-content'>\n"
        content += "<table class='transposable'>\n"
        content += "<tr>\n"
        content += th_class_prefix+"Remaining Capacity</th>\n";
        content += th_class_prefix+"Status</th>\n";
        content += "</tr>\n"
        content += "<tr>\n"
        content += "<td>" + displayAltStringIfEmpty(data.Battery.ChargePercent) + " %</td>\n";
        content += "<td>" + displayAltStringIfEmpty(data.Battery.Status) + "</td>\n";
        content += "</tr>\n"
        content += "</table>\n"
        content += "</div>\n"
    }
#endif

    return content
}

// Page 2 top part (current RF scan)
// Generate table with current RF data
function buildScanResultTable(data) {
    var scanResultTable = "<div class='scan-table'>";
    rsrp_unit_str = " (" + data.limits.RSRP.unit + ") ";
    rsrq_unit_str = " (" + data.limits.RSRQ.unit + ") ";
    scanResultTable += "<div class='scan-main-header row-wrap'>";
    scanResultTable += "<div class='left-res'>";
    scanResultTable += "<div class='scan-cell res-pci'>PCI</div>";
    scanResultTable += "<div class='scan-cell res-earfcn'>EARFCN</div>";
    scanResultTable += "<div class='scan-cell a-center topdivider res-ecgi'>";
    scanResultTable += data.show_ecgi_or_eci ? "ECGI" : "ECI";
    scanResultTable += "</div>";
    scanResultTable += "</div>"; // left-res
    scanResultTable += "<div class='right-res'>";
    scanResultTable += "<div class='scan-cell a-center res-measure'>Measure</div>";
    scanResultTable += "<div class='scan-cell a-center res-value'>Value</div>";
    scanResultTable += "</div>"; // right-res
    scanResultTable += "<div class='clr'></div>";
    scanResultTable += "</div>"; // scan-main-header

    scanResultTable += "<div class='scan-data-content'>";
    for (var i = 0; i < data.current_readings.length; i++) {
        scanResultTable += "<div class='row-wrap'>";
        scanResultTable += "<div class='left-res'>";
        scanResultTable += "<div class='scan-cell res-pci'>" + data.current_readings[i].pci + "</div>";
        scanResultTable += "<div class='scan-cell res-earfcn'>" + data.current_readings[i].earfcn + "</div>";
        scanResultTable += "<div class='scan-cell a-center topdivider res-ecgi'>" + displayEcgiOrEci(data.current_readings[i].ecgi, data.show_ecgi_or_eci) + "</div>";
        scanResultTable += "</div>"; // left-res
        scanResultTable += "<div class='right-res'>";
        scanResultTable += "<div class='scan-cell res-measure'>RSRP " + rsrp_unit_str + "</div>";
        scanResultTable += "<div class='scan-cell res-value'>" + data.current_readings[i].rsrp + "</div>";
        if (data.current_readings[i].rssinr) {
            scanResultTable += "<div class='scan-cell res-measure'>RSSINR " + rsrq_unit_str + "</div>";
            scanResultTable += "<div class='scan-cell res-value'>" + data.current_readings[i].rssinr + "</div>";
        }
        scanResultTable += "<div class='scan-cell res-measure'>RSRQ " + rsrq_unit_str + "</div>";
        scanResultTable += "<div class='scan-cell res-value'>" + data.current_readings[i].rsrq + "</div>";
        scanResultTable += "</div>"; // right-res
        scanResultTable += "<div class='clr'></div>";
        scanResultTable += "</div>\n"; // row-wrap
    }
    scanResultTable += "</div>"; // scan-data-content
    scanResultTable += "</div>"; // scan-table
    return scanResultTable
}

// Page 2 - Stats section
// Generate table with RF stats data
function buildStatsResultTable(data) {
    var statsTable = "<div class='scan-table'>";

#ifdef V_ORIENTATION_y
    function isOrientationAccurate(accuracy) {
        if (accuracy == 'High' || accuracy == 'Medium') {
            return true;
        }
        return false;
    }
#endif

    statsTable += "<div class='scan-main-header row-wrap'>"; // main header
    statsTable += "<div class='left-stats'>\n";
    statsTable += "<div class='scan-cell stats-pci'>PCI</div>";
    statsTable += "<div class='scan-cell stats-earfcn'>EARFCN</div>";
    statsTable += "<div class='scan-cell stats-samples'>No.</div>";
    statsTable += "<div class='scan-cell a-center topdivider stats-ecgi'>";
    statsTable += data.show_ecgi_or_eci ? "ECGI" : "ECI";
    statsTable += "</div>\n";
    statsTable += "</div>\n"; // left-stats
    statsTable += "<div class='right-stats'>\n";
    statsTable += "<div class='scan-cell a-center stats-rfgroup'>RSRP</div>";
    statsTable += "<div class='scan-cell topdivider a-center stats-rfgroup'>RSRQ</div>\n";
    statsTable += "</div>\n"; // right-stats
#ifdef V_ORIENTATION_y
    statsTable += "<div class='ori-stats'>\n";
    statsTable += "<div class='scan-cell topdivider a-center stats-origroup'>Orientation</div>\n";
    statsTable += "</div>\n"; // ori-stats
#endif
    statsTable += "<div class='clr'></div>\n";
    statsTable += "</div>\n"; // scan-main-header

    // the secondary (min/max) header
    statsTable += "<div class='scan-secondary-header row-wrap'>\n";
    statsTable += "<div class='left-stats'>&nbsp;</div>\n";
    statsTable += "<div class='right-stats'>\n";
    statsTable += "<div class='stats-rfgroup'>";
    statsTable += "<div class='scan-cell stats-rsrx'>MIN</div>";
    statsTable += "<div class='scan-cell stats-rsrx'>MAX</div>";
    statsTable += "<div class='scan-cell stats-rsrx'>AVG</div>";
    statsTable += "</div>"; // stats-rfgroup
    statsTable += "<div class='stats-rfgroup hidable'>";
    statsTable += "<div class='scan-cell stats-rsrx'>MIN</div>";
    statsTable += "<div class='scan-cell stats-rsrx'>MAX</div>";
    statsTable += "<div class='scan-cell stats-rsrx'>AVG</div>";
    statsTable += "</div>\n"; // stats-rfgroup
    statsTable += "</div>\n"; // right-stats
#ifdef V_ORIENTATION_y
    statsTable += "<div class='ori-stats'>\n";
    statsTable += "<div>Best Azimuth</div>\n";
    statsTable += "<div>Best Elevation</div>\n";
    statsTable += "</div>\n"; // ori-stats
#endif
    statsTable += "<div class='clr'></div>\n";
    statsTable += "</div>\n"; // secondary header

    statsTable += "<div class='scan-data-content'>\n";
    for (var i = 0; i < data.stats.length; i++) {
        statsTable += "<div class='row-wrap'>\n";
        statsTable += "<div class='left-stats'>\n";
        statsTable += "<div class='scan-cell stats-pci'>";
        statsTable += data.stats[i].pci;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell stats-earfcn'>";
        statsTable += data.stats[i].earfcn;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell stats-samples'>";
        statsTable += data.stats[i].ns;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell a-center topdivider stats-ecgi'>";
        statsTable += displayEcgiOrEci(data.stats[i].ecgi, data.show_ecgi_or_eci);
        statsTable += "</div>\n";
        statsTable += "</div>\n"; // left-stats

        statsTable += "<div class='right-stats'>\n";
        statsTable += "<div class='stats-rfgroup'>\n";
        statsTable += "<div class='scan-cell stats-rsrx'>";
        statsTable += data.stats[i].rsrp_min;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell stats-rsrx'>";
        statsTable += data.stats[i].rsrp_max;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell stats-rsrx'>";
        statsTable += data.stats[i].rsrp_avg;
        statsTable += "</div>";
        statsTable += "</div>"; // stats-rfgroup
        statsTable += "<div class='stats-rfgroup topdivider'>";
        statsTable += "<div class='scan-cell stats-rsrx'>";
        statsTable += data.stats[i].rsrq_min;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell stats-rsrx'>";
        statsTable += data.stats[i].rsrq_max;
        statsTable += "</div>";
        statsTable += "<div class='scan-cell stats-rsrx'>";
        statsTable += data.stats[i].rsrq_avg;
        statsTable += "</div>";
        statsTable += "</div>\n"; // stats-rfgroup
        statsTable += "</div>\n"; // right-stats
#ifdef V_ORIENTATION_y
        statsTable += "<div class='ori-stats'>\n";
        if (data.stats[i].orientation &&
            isOrientationAccurate(data.stats[i].orientation.accuracy)) {
            statsTable += "<div class='scan-cell'>";
            statsTable += displayAltStringIfEmpty(data.stats[i].orientation.azimuth)
            statsTable += " &deg;</div>\n";
            statsTable += "<div class='scan-cell'>";
            statsTable += displayAltStringIfEmpty(data.stats[i].orientation.elevation)
            statsTable += " &deg;</div>\n";
        } else {
            statsTable += "<div class='scan-cell'>N/A</div>";
            statsTable += "<div class='scan-cell'>N/A</div>";
        }
        statsTable += "</div>\n"; // ori-stats
#endif
        statsTable += "<div class='clr'></div>\n";
        statsTable += "</div>\n"; // row-wrap
    }
    statsTable += "</div>\n"; // scan-data-content
    statsTable += "</div>"; // scan-table
    return statsTable;
}

// Special page - no valid RF data
function buildInvalidResult(data) {
    var res = "<p>" + data.message + "</p";
    return res;
}
