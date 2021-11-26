/*
Copyright (C) 2019 NetComm Wireless Limited.

Class processing live stream orientation data
*/

/*
 * Constructor
 * @param statusElId Element ID of orientation status
 * @param azimuthElId Element ID of azimuth data
 * @param downTiltElId Element ID of downtilt data
 * @param bearingElId Element ID of bearing data
 */
function OrientationDataHandler(statusElId, azimuthElId, downTiltElId, bearingElId) {
    this.statusElId = statusElId ? statusElId : "orientation_status";
    this.azimuthElId = azimuthElId ? azimuthElId : "azimuth_data";
    this.downTiltElId = downTiltElId ? downTiltElId : "down_tilt_data";
    this.bearingElId = bearingElId ? bearingElId : "bearing_data";
    // to store received data
    this.orientationStatus = "";
    this.azimuthData = "";
    this.downTiltData = "";
    this.bearingData = "";
}

/*
 * Handle orientation data
 * @param data Orientation data
 */
OrientationDataHandler.prototype.handleData = function(data) {
    var statusEl = document.getElementById(this.statusElId);
    var azimuthEl = document.getElementById(this.azimuthElId);
    var downTiltEl = document.getElementById(this.downTiltElId);
    var bearingEl = document.getElementById(this.bearingElId);
    this.orientationStatus = data.orientation_status;
    if ((data.orientation_status == "0" || data.orientation_status == "2")
            && data.down_tilt && data.bearing) {
        if (statusEl) {
            statusEl.textContent = "Calibrated";
        }
        // data is always number so innerHTML should be fine
        if (downTiltEl) {
            downTiltEl.innerHTML = data.down_tilt + "&deg;";
        }
        if (bearingEl) {
            bearingEl.innerHTML = data.bearing + "&deg;";
        }
        this.downTiltData = data.down_tilt;
        this.bearingData = data.bearing;
    } else {
        if (data.orientation_status == "1") {
            if (statusEl) {
                statusEl.textContent = "Calibrating";
            }
        } else {
            if (statusEl) {
                statusEl.textContent = "Error";
            }
        }
        if (downTiltEl) {
            downTiltEl.innerHTML = "Not available";
        }
        if (bearingEl) {
            bearingEl.innerHTML = "Not available";
        }
        this.downTiltData = "";
        this.bearingData = "";
    }

    if (data.orientation_status == "0" && data.azimuth) {
        if (azimuthEl) {
            azimuthEl.innerHTML = data.azimuth + "&deg;";
        }
        this.azimuthData = data.azimuth;
    } else {
        if (azimuthEl) {
            azimuthEl.textContent = "Not available";
        }
        this.azimuthData = ""
    }
}

/*
 * OrientationGpsDataHandler: Derived class handling both orientation and GPS data
 * Constructor of OrientationGpsDataHandler
 *
 * @param statusElId Element ID of orientation status
 * @param azimuthElId Element ID of azimuth data
 * @param downTiltElId Element ID of downtilt data
 * @param bearingElId Element ID of bearing data
 * @param latitudeElId Element ID of latitude data
 * @param longitudeElId Element ID of longitude data
 * @param heightElId Element ID of height data
 * @param heightTypeElId Element ID of height type
 * @param copyBtnId Element ID of the button to copy data to intput fields
 */
function OrientationGpsDataHandler(statusElId, azimuthElId, downTiltElId, bearingElId,
    latitudeElId, longitudeElId, heightElId, heightTypeElId, copyBtnId) {
    OrientationDataHandler.call(this, statusElId, azimuthElId, downTiltElId, bearingElId);
    this.latitudeElId = latitudeElId ? latitudeElId : "latitude_data";
    this.longitudeElId = longitudeElId ? longitudeElId : "longitude_data";
    this.heightElId = heightElId ? heightElId : "height_data";
    this.heightTypeElId = heightTypeElId ? heightTypeElId : "height_type";
    this.copyBtnId = copyBtnId ? copyBtnId : "copy_btn";
    // to store gps data
    this.latitudeData = "";
    this.longitudeData = "";
    this.heightData = "";
    this.heightDataType = "";
}

OrientationGpsDataHandler.prototype = Object.create(OrientationDataHandler.prototype);

/*
 * Handle data
 * @param data Orientation and GPS data
 */
OrientationGpsDataHandler.prototype.handleData = function(data) {
    OrientationDataHandler.prototype.handleData.call(this, data);
    var latitudeEl = document.getElementById(this.latitudeElId);
    var longitudeEl = document.getElementById(this.longitudeElId);
    var heightEl = document.getElementById(this.heightElId);
    var heightTypeEl = document.getElementById(this.heightTypeElId);
    if (data.latitude && data.longitude && data.height) {
        // data is always number so innerHTML should be fine
        if (latitudeEl) {
            latitudeEl.innerHTML = data.latitude + "&deg;";
        }
        if (longitudeEl) {
            longitudeEl.innerHTML = data.longitude + "&deg;";
        }
        if (heightEl) {
            heightEl.innerHTML = data.height + " m";
            if (heightTypeEl) {
                heightTypeEl.innerHTML = "AMSL";
            }
        }
        this.latitudeData = data.latitude;
        this.longitudeData = data.longitude;
        this.heightData = data.height;
        this.heightDataType = "AMSL"; // GPS height type is AMSL
    } else {
        if (latitudeEl) {
            latitudeEl.innerHTML = "Not available";
        }
        if (longitudeEl) {
            longitudeEl.innerHTML = "Not available";
        }
        if (heightEl) {
            heightEl.innerHTML = "Not available";
            if (heightTypeEl) {
                heightTypeEl.innerHTML = "Not available";
            }
        }
        this.latitudeData = "";
        this.longitudeData = "";
        this.heightData = "";
        this.heightDataType = "";
    }

    var copyBtnEl = document.getElementById(this.copyBtnId);
    if (copyBtnEl) {
        if (this.isDataValid()) {
            copyBtnEl.disabled = false;
        } else {
            copyBtnEl.disabled = true;
        }
    }
}

/*
 * Check whether current received data is valid to enable copying to input fields
 * @retval true if valid, false otherwise
 */
OrientationGpsDataHandler.prototype.isDataValid = function() {
    return this.orientationStatus === "0" && this.latitudeData && this.longitudeData && this.heightData;
}
