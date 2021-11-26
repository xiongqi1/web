/*
Copyright (C) 2019 NetComm Wireless Limited.

Class processing RF network scan result and playing
a notification sound when new best cell signal is detected.
*/

/*
 * Constructor
 * @param soundId sound Element ID
 * @param allowingDivId Element ID of div containing button asking users to allow playing sound
 */
function RfScanProcessor(soundId, allowingDivId) {
    if (typeof soundId === 'undefined') {
        this.soundId = "audio_indicator";
    } else {
        this.soundId = soundId;
    }
    if (typeof allowingDivId === 'undefined') {
        this.allowingDivId = "allowing_audio_indicator";
    } else {
        this.allowingDivId = allowingDivId;
    }

    this.bestCellRsrp = -999;
    this.bestCellId = "";

    this.waitScannableIndShown = false;

    this.playSoundAllowed = false;
    this.havingGoodCells = false;
}

/*
 * Checking whether browser allow playing sound without user interaction
 * If it does not, showing a button asking for user interaction
 */
RfScanProcessor.prototype.checkPlayingSound = function() {
    var testAudio = document.createElement('audio');
    testAudio.play();
    if (testAudio.paused) {
        document.getElementById(this.allowingDivId).style["display"] = "block";
    } else {
        this.playSoundAllowed = true;
    }
}

/*
 * Handler of user click on button to allow playing sound
 * Loading the sound to be played later
 */
RfScanProcessor.prototype.allowPlaySound = function() {
    document.getElementById(this.soundId).load();
    document.getElementById(this.allowingDivId).style["display"] = "none";
    this.playSoundAllowed = true;
    if (this.havingGoodCells) {
        this.playAudioIndicator();
    }
}

/*
 * Play audio indicator
 */
RfScanProcessor.prototype.playAudioIndicator = function() {
    if (this.playSoundAllowed) {
        var sound = document.getElementById(this.soundId);
        sound.pause();
        sound.load();
        sound.play();
    }
}

/*
 * Process RF network scan result and play a notification sound
 * when it detects new best cell signal.
 * The cells to be considered must match user-provided cell sector ID and has RSRP >= pass RSRP
 *
 * @param scanResult RF scan result object received from server's web socket service
 */
RfScanProcessor.prototype.processScanResult = function(scanResult) {
    this.indicateScanable(scanResult);

    var prevBestCellDetected = false;
    var prevBestCellCurRsrp;
    var bestCellRsrp = -999;
    var bestCellId = "";
    this.havingGoodCells = false;
    for (var i = 0; i < scanResult.current_readings.length; i++) {
        var rfCell = scanResult.current_readings[i];
        if (rfCell.cell_sector_id && rfCell.rsrp >= rfCell.rsrp_pass) {
            this.havingGoodCells = true;
        }
        if (rfCell.cell_sector_id && rfCell.rsrp >= rfCell.rsrp_pass && rfCell.rsrp > bestCellRsrp) {
            bestCellRsrp = rfCell.rsrp;
            bestCellId = rfCell.cell_sector_id;
        }
        if (rfCell.cell_sector_id && this.bestCellId === rfCell.cell_sector_id) {
            prevBestCellDetected = true;
            if (rfCell.rsrp >= rfCell.rsrp_pass) {
                this.bestCellRsrp = rfCell.rsrp;
            } else {
                this.bestCellRsrp = -999;
                this.bestCellId = "";
            }
            prevBestCellCurRsrp = rfCell.rsrp;
        }
    }

    if (!prevBestCellCurRsrp) {
        this.bestCellRsrp = -999;
        this.bestCellId = "";
    }

    if (bestCellId == this.bestCellId) {
        // best cell is the same, or no good cell detected
        // no play sound
        // if best cell is the same, rsrp should be already updated
    } else {
        // best cell is different, play sound
        this.playAudioIndicator();
        this.bestCellRsrp = bestCellRsrp;
        this.bestCellId = bestCellId;
    }
}

/*
 * Process scannable property in RF network scan data to decide
 * showing waiting scannable state indicator
 */
RfScanProcessor.prototype.indicateScanable = function(scanResult) {
    var waitScannableInd = document.getElementById("wait_scannable_state_ind");

    if (waitScannableInd) {
        if (scanResult.scannable && this.waitScannableIndShown) {
            waitScannableInd.setAttribute("style", "display:none");
            this.waitScannableIndShownTime = undefined;
            this.waitScannableIndShown = false;
        } else if (!scanResult.scannable && !this.waitScannableIndShown) {
            waitScannableInd.setAttribute("style", "display:block");
            this.waitScannableIndShown = true;
        }

        if (!this.modemDetachAutoRequested) {
            var now = new Date();
            if (this.waitScannableIndShownTime) {
                var duration = (now.getTime() - this.waitScannableIndShownTime.getTime()) / 1000;
                if (duration > 15) {
                    ajaxPostRequest("/modem_detach");
                    this.modemDetachAutoRequested = true;
                }
            }
            else {
                this.waitScannableIndShownTime = now;
            }
        }
    }
}

/*
 * Helper function to find next sibling of an element
 * @param element given element to find
 * @return next sibling element of given element
 */
function nextElementSibling(element) {
    do {
        element = element.nextSibling;
    } while (element !== null && element.nodeType !== 1);
    return element;
}

/*
 * Helper function to find previous sibling of an element
 * @param element given element to find
 * @return previous sibling element of given element
 */
function previousElementSibling(element) {
    do {
        element = element.previousSibling;
    } while (element !== null && element.nodeType !== 1);
    return element;
}

/*
 * Helper function to find first child of an element
 * @param element given element to find
 * @return first element child of given element
 */
function firstElementChild(element) {
    var child = element.firstChild;
    while (child !== null && child.nodeType !== 1) {
        child = child.nextSibling;
    }
    return child;
}

/*
 * Helper function to find last child of an element
 * @param element given element to find
 * @return last element child of given element
 */
function lastElementChild(element) {
    var child = element.lastChild;
    while (child !== null && child.nodeType !== 1) {
        child = child.previousSibling;
    }
    return child;
}

/*
 * AdvRfScanProcessor: derived from RfScanProcessor class to provide redering scan
 * result table in Advanced Operation page
 */

/* Constructor of AdvRfScanProcessor */
function AdvRfScanProcessor(soundId, allowingDivId) {
    RfScanProcessor.call(this, soundId, allowingDivId);

    this._scanRenderTable = document.getElementById("advScanResult");
}

AdvRfScanProcessor.prototype = Object.create(RfScanProcessor.prototype);
AdvRfScanProcessor.prototype.constructor = AdvRfScanProcessor;

/*
 * Show all cells regardless of plmn filter
 * @param show whether to show or hide
 */
AdvRfScanProcessor.prototype.showAllCells = function(show) {
    this._showAllCells = show;
}

/*
 * @copydoc RfScanProcessor::processScanResult
 * Rendering scan result table
 */
AdvRfScanProcessor.prototype.processScanResult = function(scanResult) {
    RfScanProcessor.prototype.processScanResult.call(this, scanResult);
    if(this._showAllCells) {
        scanResult.current_readings = scanResult.current_readings.concat(scanResult.extra_readings);
    }
    delete scanResult.extra_readings;

    this.renderScanResult(scanResult);
}

/*
 * Set handler function name to be called on click on Save button of each cell
 * The first argument of the handler function is element of clicked Save button.
 * @param functionName handler function name as a string
 */
AdvRfScanProcessor.prototype.setSaveButtonHandler = function(functionName) {
    this._saveButtonHandlerName = functionName;
}

/*
Each cell is represented in a row in a table with following layout:
    <tr>
        <td>310410089303189</td>    <!-- cell sector ID -->
        <td>*</td>    <!-- * indicates serving cell -->
        <td>-98 (dBm)</td>    <!-- minimum RSRP i.e RSRP pass threshold -->
        <td>
            <div>
                <div class="fixed_field_width table_td">RSRP (dBm)</div>
                <div class="table_td green-font">-80</div>
            </div>
            <div>
                <div class="fixed_field_width table_td">RSSINR (dB)</div>
                <div class="table_td">0</div>
            </div>
            <div>
                <div class="fixed_field_width table_td">RSRQ (dB)</div>
                <div class="table_td">-13</div>
            </div>
        </td>
        <td><button class="tinyfont">Save</button></td>
    </tr>
*/

/*
 * Create a data TD element
 * @param rowElement parent row element
 * @param isButton true indicates this TD is for Save button
 * @param value If provided, set text value for the TD
 * @param styleClass If provided, set CSS class for the TD
 * @return created TD element
 */
AdvRfScanProcessor.prototype.createScanDataTd = function(rowElement, isButton, value, styleClass) {
    var td = document.createElement("td");
    if (styleClass) {
        td.setAttribute("class", styleClass);
    }
    if (isButton) {
        td.innerHTML = "<button class=\"tinyfont\" onclick=\"" + this._saveButtonHandlerName + "(this)\">Save</button>";
    } else {
        if (typeof value !== "undefined") {
            td.textContent = value;
        }
    }
    rowElement.appendChild(td);

    return td;
}

/*
 * Client's Save button handler calls this method to get information of the corresponding cell for the clicked button.
 * Return an object:
 * {
 *     cellId: Cell sector ID value
 *     rsrp: RSRP
 *     goodRsrp: Whether RSRP is over pass threshold
 *     rssinr: RSSINR
 *     rsrq: RSRQ
 * }
 *
 * @param buttonElement Save button element
 * @return described object
 */
AdvRfScanProcessor.prototype.saveButtonHandler = function(buttonElement) {
    var cellRow = buttonElement.parentNode.parentNode;
    var cellIdTd = firstElementChild(cellRow);
    var rfTd = previousElementSibling(buttonElement.parentNode);

    var rsrpDiv = firstElementChild(rfTd);
    var rsrpValDiv = lastElementChild(rsrpDiv);
    var res = /(\w+)-font$/.exec(rsrpValDiv.getAttribute("class"));
    var goodRsrp = false;
    if (res && res[1] === "green") {
        goodRsrp = true;
    }
    var rsrp = rsrpValDiv.textContent;

    var rssinrDiv = nextElementSibling(rsrpDiv);
    var rssinr = lastElementChild(rssinrDiv).textContent;

    var rsrqDiv = nextElementSibling(rssinrDiv);
    var rsrq = lastElementChild(rsrqDiv).textContent;

    return {cellId: cellIdTd.textContent, rsrp: rsrp, goodRsrp: goodRsrp, rssinr: rssinr, rsrq: rsrq};
}

/*
 * Create RF data DIV
 * @param tdElement Parent element
 * @param hidden Whether the field will be hidden
 * @param label Label
 * @param value Value
 * @param labelCssClass CSS class for label
 * @param valueCssClass CSS class for value
 */
AdvRfScanProcessor.prototype.createRfDataDiv = function(tdElement, hidden, label, value, labelCssClass, valueCssClass) {
    var div = document.createElement("div");
    if (hidden) {
        div.setAttribute("style", "display:none");
    }

    var labelDiv = document.createElement("div");
    var valueDiv = document.createElement("div");
    labelDiv.setAttribute("class", labelCssClass);
    valueDiv.setAttribute("class", valueCssClass);
    labelDiv.textContent = label;
    valueDiv.textContent = value;
    div.appendChild(labelDiv);
    div.appendChild(valueDiv);
    tdElement.appendChild(div);
}

/*
 * Create new cell row
 * @param rfCellData Cell data object
 * @param rsrpUnit RSRP unit string
 * @param rsrqUnit RSRQ unit string
 * @return created row TR element
 */
AdvRfScanProcessor.prototype.newCellRow = function(rfCellData, rsrpUnit, rsrqUnit) {
    var row = document.createElement("tr");
    this.createScanDataTd(row, false, rfCellData.cellIdText);
    this.createScanDataTd(row, false, rfCellData.servingCellMark);
    this.createScanDataTd(row, false, rfCellData.minRsrp);
    var rfTd = this.createScanDataTd(row, false);
    this.createScanDataTd(row, true);

    this.createRfDataDiv(rfTd, false, "RSRP" + rsrpUnit, rfCellData.rsrp, "fixed_field_width table_td",
        rfCellData.rsrp < rfCellData.rsrp_pass ? "table_td red-font" : "table_td green-font");
    this.createRfDataDiv(rfTd, !rfCellData.isServingCell, "RSSINR" + rsrqUnit, rfCellData.isServingCell ? rfCellData.rssinr : "", "fixed_field_width table_td", "table_td");
    this.createRfDataDiv(rfTd, false, "RSRQ" + rsrqUnit, rfCellData.rsrq, "fixed_field_width table_td", "table_td");

    return row;
}

/*
 * Update element value
 * @param element Element to update
 * @param value New value
 * @param cssClass New CSS class
 * @param style New CSS style
 */
AdvRfScanProcessor.prototype.updateElementValue = function(element, value, cssClass, style) {
    if (element.textContent !== value) {
        element.textContent = value;
    }
    if (typeof cssClass !== "undefined") {
        element.setAttribute("class", cssClass);
    }
    if (typeof style !== "undefined") {
        element.setAttribute("style", style);
    }
}

/*
 * Check whether a row element represents a cell identified by given cell sector ID
 * @param cellRenderRow Row element to check
 * @param cellId Cell sector ID to check
 * @return true/false
 */
AdvRfScanProcessor.prototype.isElementForCell = function(cellRenderRow, cellId) {
    return firstElementChild(cellRenderRow).textContent === cellId.toString();
}

/*
 * Update a cell row element
 * @param renderRow Row to be updated
 * @param rfCellData New cell data object
 * @param rsrpUnit RSRP unit string
 * @param rsrqUnit RSRQ unit string
 */
AdvRfScanProcessor.prototype.updateCellRow = function(renderRow, rfCellData, rsrpUnit, rsrqUnit) {
    var td = firstElementChild(renderRow);
    this.updateElementValue(td, rfCellData.cellIdText);

    td = nextElementSibling(td);
    this.updateElementValue(td, rfCellData.servingCellMark);

    td = nextElementSibling(td);
    this.updateElementValue(td, rfCellData.minRsrp);

    var rfTd = nextElementSibling(td);

    var rsrpDiv = firstElementChild(rfTd);
    var rsrpValDiv = nextElementSibling(firstElementChild(rsrpDiv));
    this.updateElementValue(rsrpValDiv, rfCellData.rsrp, rfCellData.rsrp < rfCellData.rsrp_pass ? "table_td red-font" : "table_td green-font");

    var rssinrDiv = nextElementSibling(rsrpDiv);
    var rssinrValDiv = nextElementSibling(firstElementChild(rssinrDiv));
    if (rfCellData.isServingCell) {
        this.updateElementValue(rssinrValDiv, rfCellData.rssinr, "table_td");
        rssinrDiv.setAttribute("style", "");
    } else {
        this.updateElementValue(rssinrValDiv, "", "table_td");
        rssinrDiv.setAttribute("style", "display:none");
    }

    var rsrqDiv = nextElementSibling(rssinrDiv);
    var rsrqValDiv = nextElementSibling(firstElementChild(rsrqDiv));
    this.updateElementValue(rsrqValDiv, rfCellData.rsrq);
}

/*
 * Render scan result table
 * @param scanResult Scan result data provided by server
 */
AdvRfScanProcessor.prototype.renderScanResult = function(scanResult) {

    var rsrpUnit = " (" + scanResult.limits.RSRP.unit + ")";
    var rsrqUnit = " (" + scanResult.limits.RSRQ.unit + ")";

    var renderTable = this._scanRenderTable;
    // skip the first child, which is for header
    var cellRenderRow = nextElementSibling(firstElementChild(renderTable));

    for (var i = 0; i < scanResult.current_readings.length; i++) {
        var rfCell = scanResult.current_readings[i];
        rfCell.cellIdText = rfCell.cell_sector_id ? rfCell.cell_sector_id.toString() : (rfCell.pci.toString() + " (PCI)");
        if (rfCell.hasOwnProperty("heading")) {
            // append degree unit
            rfCell.heading = rfCell.heading + "\u00B0";
        } else {
            rfCell.heading = "";
        }
        if ((rfCell.cell_sector_id > 0) && (rfCell.cell_sector_id == rfCell.serving_cell)) {
            rfCell.servingCellMark = "*";
            rfCell.isServingCell = true;
        } else {
            rfCell.servingCellMark = " ";
            rfCell.isServingCell = false;
        }
        rfCell.minRsrp = rfCell.rsrp_pass + rsrpUnit;

        if (cellRenderRow === null) {
            // insert new
            cellRenderRow = this.newCellRow(rfCell, rsrpUnit, rsrqUnit);
            cellRenderRow = renderTable.appendChild(cellRenderRow);
        } else {
            // find existing
            if (this.isElementForCell(cellRenderRow, rfCell.cellIdText)) {
               // update
               this.updateCellRow(cellRenderRow, rfCell, rsrpUnit, rsrqUnit);
            } else {
                var checkingRow = cellRenderRow;
                while (checkingRow && !this.isElementForCell(checkingRow, rfCell)) {
                    checkingRow = nextElementSibling(checkingRow);
                }

                if (!checkingRow) {
                    // not found: insert new
                    var newCellRow = this.newCellRow(rfCell, rsrpUnit, rsrqUnit);
                    cellRenderRow = renderTable.insertBefore(newCellRow, cellRenderRow);
                } else {
                    // found: update and move to current position
                    this.updateCellRow(checkingRow, rfCell, rsrpUnit, rsrqUnit);
                    cellRenderRow = renderTable.insertBefore(checkingRow, cellRenderRow);
                }
            }
        }

        cellRenderRow = nextElementSibling(cellRenderRow);
    }

    // delete the rest
    while (cellRenderRow) {
        var nextRow = nextElementSibling(cellRenderRow);
        renderTable.removeChild(cellRenderRow);
        cellRenderRow = nextRow;
    }
}
