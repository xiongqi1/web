function dBmFmt(v, units, thres) {
    if (!isDefined(thres))
      thres = -120;
    var val=parseInt(v);
    if (val != NaN && val <= thres)
      return _("na");
    return v + units;
}

  var advStatus = PageObj("StsAdvStatus", "advStatus advancedStatus",
    {
  /*    customLua: {
        lockRdb: false,
        get: function(arr) {
          arr.push("for line in io.lines('/etc/version.txt') do o.version = line break end");
          return arr;
        },
      },
  */
      readOnly: true,
      column: 1,
      genObjHtml: genCols,
      pollPeriod: 1000,

      columns : [{
        members:[
          {hdg: "country code", genHtml: (obj) => obj.MCC},
          {hdg: "advStatus networkCode", genHtml: (obj) => pad(obj.MNC, 2)},
          {hdg: "signalQualityEcIo",
            genHtml: (obj) => obj.ecios_val,
            isVisible: (obj) => {
              /* show or hide ecn0 and ecio */
              var ecn0_mode = obj.ECN0_valid == "1";

              if(!ecn0_mode) {
                if(  isValidValue(obj.ecio0) ||  isValidValue(obj.ecio1)) {
                  obj.ecios_val = _("carrier")+" 0:&nbsp;&nbsp;&nbsp;&nbsp;" + obj.ecio0 + "<br/>"+_("carrier")+" 1:&nbsp;&nbsp;&nbsp;&nbsp;" + obj.ecio1;
                  return true;
                }
                if (isValidValue(obj.ecio0) && obj.ECIOs0 != 0) {
                  obj.ecios_val = "-" + obj.ECIOs0 + " dB";
                  return true;
                }
                return false;
              }
              return false;
            }
          },
          {hdg: "signalQualityEcN0",
            genHtml: (obj) => isValidValue(obj.ECN0s0) && obj.ECN0s0 != 0 ? "-" + obj.ECN0s0 + " dB": _("na"),
            isVisible: (obj) => obj.ECN0_valid == "1"
        },
          {hdg: "receivedSignalCodePower",
            genHtml: (obj) =>{
              if( isValidValue(obj.rscp0) || isValidValue(obj.rscp1)) {
                return _("carrier")+" 0:&nbsp;&nbsp;&nbsp;&nbsp;" + dBmFmt(obj.rscp0, " dBm", -109) + "<br/>"+_("carrier")+" 1:&nbsp;&nbsp;&nbsp;&nbsp;" + dBmFmt(obj.rscp1, " dBm", -109);
              }
              return dBmFmt(-Number(obj.RSCPs0), " dBm", -109);
            },
            isVisible: (obj) => isValidValue(obj.rscp0) || isValidValue(obj.rscp1) || isValidValue(obj.rsrp0) || (isValidValue(obj.RSCPs0) && obj.RSCPs0!='0')
          },
          {hdg: "dcInputVoltage",
            genHtml: (obj) => obj.dcvoltage != 0 ? obj.dcvoltage + "V" : _("na"),
            isVisible: (obj) => isValidValue(obj.dcvoltage)},
          {hdg: "rsrq", genHtml: (obj) => dBmFmt(obj.rsrq, " dB"), isVisible: (obj) => isValidValue(obj.rsrq)},
          {hdg: _("rsrp") + " 0", genHtml: (obj) => dBmFmt(obj.rsrp0, " dB"), isVisible: (obj) => isValidValue(obj.rsrp0)},
          {hdg: _("rsrp") + " 1", genHtml: (obj) => dBmFmt(obj.rsrp1, " dB"), isVisible: (obj) => isValidValue(obj.rsrp1)},
          {hdg: "rssi", genHtml: (obj) => dBmFmt(obj.rsrp1, " dBm", -109),
            genHtml: (obj) =>{ // This comes from the other page object
              return dBmFmt(cellularConnectionStatus.obj.csq, " dBm", -109);
            },
            isVisible: (obj) => !isValidValue(obj.rscp0) && !isValidValue(obj.rscp1) && obj.RSCPs0=='0' && !isValidValue(obj.rsrp0) && isValidValue(obj.rsrp1)
          }
        ]
      },
      {
        members:[
          {hdg: "hsupaCategory",
            genHtml: (obj) => obj.hsucat,
            isVisible: (obj) => {  // This comes from the other page object
              var connType = cellularConnectionStatus.obj.connType;
              return connType.substring(0, 3) != "GSM" && connType.substring(0, 4) != "EDGE" && connType.substring(0, 5) != "EGPRS";
            }
        },
          {hdg: "hsdpaCategory",
            genHtml: (obj) => obj.hsdcat,
            isVisible: (obj) => {  // This comes from the other page object
              var connType = cellularConnectionStatus.obj.connType;
              return connType.substring(0, 3) != "GSM" && connType.substring(0, 4) != "EDGE" && connType.substring(0, 5) != "EGPRS";
            }
          },
          {hdg: "advStatus iccid", genHtml: (obj) => isValidValue(obj.PSCs0)? obj.simICCID: _("na")},
          {hdg: "primaryScramblingCode", genHtml: (obj) => obj.PSCs0, isVisible: (obj) => isValidValue(obj.PSCs0)},
          {hdg: "powerSource", genHtml: (obj) => obj.powersource, isVisible: (obj) => isValidValue(obj.powersource)},
          {hdg: "advStatus locationAreaCode", genHtml: (obj) => obj.LAC, isVisible: (obj) => isValidValue(obj.LAC)},
          {hdg: "advStatus routingAreaCode", genHtml: (obj) => obj.RAC, isVisible: (obj) => isValidValue(obj.RAC)},
          {hdg: "advStatus imsi", genHtml: (obj) => obj.IMSI}
        ]
      },
      {
        members:[
          {hdg: "advStatus cellID", genHtml: (obj) => obj.cellId},
          {hdg: "advStatus channelNumber UARFCN",
            genHtml: (obj) => obj.ChannelNumber,
            isVisible: (obj) => {  // This comes from the other page object
              var connType = cellularConnectionStatus.obj.connType;
              return !(connType.substring(0, 3) == "GSM" || connType.substring(0, 4) == "EDGE");
            }
          },
          {hdg: "advStatus channelNumber ARFCN",
            genHtml: (obj) => obj.ChannelNumber,
            isVisible: (obj) => {  // This comes from the other page object
              var connType = cellularConnectionStatus.obj.connType;
              return connType.substring(0, 3) == "GSM" || connType.substring(0, 4) == "EDGE";
            }
          },
          {hdg: _("advStatus modulePRIID")+" "+_("Revision"), genHtml: (obj) => obj.PRIID_REV, isVisible: (obj) => isValidValue(obj.PRIID_REV)},
          {hdg: _("advStatus modulePRIID")+" "+_("PRI part number"), genHtml: (obj) => obj.PRIID_PN, isVisible: (obj) => isValidValue(obj.PRIID_PN)}
  #if defined(V_MODULE_MC7354) || defined(V_MODULE_MC7304)
          ,
          {hdg: "priid carrier"},
          {hdg: "priid config"}
  #endif
        ]
      }
      ],

      populate: function () {
        _populateCols(this.columns, this.obj);
      },

      members: [
        hiddenVariable("MCC", "wwan.0.system_network_status.MCC"),
        hiddenVariable("MNC", "wwan.0.system_network_status.MNC"),
  #ifdef V_MODCOMMS_y
        hiddenVariable("dcvoltage", "sys.sensors.info.DC_voltage"), // IoMgr will put the active source here
  #else
        hiddenVariable("dcvoltage", "sys.sensors.io.vin.adc"),
  #endif
        hiddenVariable("powersource", "sys.sensors.info.powersource"),
        hiddenVariable("cellId", "wwan.0.system_network_status.CellID"),
        hiddenVariable("IMSI", "wwan.0.imsi.msin"),
        hiddenVariable("LAC", "wwan.0.system_network_status.LAC"),
        hiddenVariable("RAC", "wwan.0.system_network_status.RAC"),
        hiddenVariable("ECN0_valid", "wwan.0.system_network_status.ECN0_valid"),
        hiddenVariable("ECN0s0", "wwan.0.system_network_status.ECN0s0"),
        hiddenVariable("ECN0s1", "wwan.0.system_network_status.ECN0s1"),
        hiddenVariable("RSCPs0", "wwan.0.system_network_status.RSCPs0"),
        hiddenVariable("rsrp0", "wwan.0.signal.0.rsrp"),
        hiddenVariable("rsrp1", "wwan.0.signal.1.rsrp"),
        hiddenVariable("rsrq", "wwan.0.signal.rsrq"),
        hiddenVariable("tac", "wwan.0.radio.information.tac"),
        hiddenVariable("pci", "wwan.0.system_network_status.pci"),
        hiddenVariable("hsucat", "wwan.0.radio.information.hsucat"),
        hiddenVariable("hsdcat", "wwan.0.radio.information.hsdcat"),
          hiddenVariable("simICCID", "wwan.0.system_network_status.simICCID"),
        hiddenVariable("PSCs0", "wwan.0.system_network_status.PSCs0"),
  #ifdef PLATFORM_Serpent
        hiddenVariable("ChannelNumber", "wwan.0.system_network_status.channel"),
  #else
        hiddenVariable("ChannelNumber", "wwan.0.system_network_status.ChannelNumber"),
  #endif
        hiddenVariable("PRIID_REV", "wwan.0.system_network_status.PRIID_REV"),
        hiddenVariable("PRIID_PN", "wwan.0.system_network_status.PRIID_PN"),

      ]
  }
);

stsPageObjects.push(advStatus);
