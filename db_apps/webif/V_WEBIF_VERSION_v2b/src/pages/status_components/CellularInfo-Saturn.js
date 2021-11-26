var registrationStr=[
    _("unregisteredNoSearch"),
    _("registeredHome"),
    _("unregisteredSearching"),
    _("registrationDenied"),
    _("unknown"),
    _("registeredRoaming"),
    _("registeredSMShome"),
    _("registeredSMSroaming"),
    _("emergency"),
    _("na")
];

var prvSimStatus = "0";
var pincounter;
var pukRetries;
var cellularConnectionStatus = PageObj("StsCellularConnectionStatus", "cellularConnectionStatus",
{
    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

    customLua: {
        lockRdb: false,
        get: function(arr) {
        return arr;
        },
    },

    columns : [{
        members:[
        {hdg: "simStatus", name: "simID"},
        {hdg: "signal strength",
            genHtml: (obj) => {
            var csq = obj.csq;
            var connType = obj.connType
            csq = csq.substring(0, csq.indexOf("dBm") );
            var csqstr = _("not available");
            var imageidx;
            if(csq == 0)
                imageidx = 6;
            else if(csq > -70 ) {
                csqstr = _("high");
                imageidx = 1;
            } else if(csq > -90 ) {
                csqstr = _("medium");
                if(csq > -80 )
                    imageidx = 2;
                else
                    imageidx = 3;
            } else if(csq > -120 ) {
                csqstr = _("low");
                if(csq > -100)
                    imageidx = 4;
                else
                    imageidx = 5;
            } else {
                imageidx = 6;
            }

            var umtsss = "&nbsp;";
            var csqImg = "&nbsp;";
            if(csq=="") {
                imageidx = 6;
            }
            else {
                umtsss = csq + " dBm     " + csqstr;
            }

            if(imageidx < 6) {
            csqImg = "<i class='connection-sml connection-sml-level" + imageidx + "'></i>";
            }
            else {
            umtsss = "";
            csqImg = "";
            }
            return '<span>' + umtsss + '</span><span>' + csqImg + '</span>';
        }},
        {hdg: "networkRegistrationStatus", name: "networkRegistration"}
        ]},
    {
        members:[
        {hdg: "operatorSelection", genHtml: (obj) => obj.operationMode},
        {hdg: "provider",
            genHtml: (obj) => {
            var provider = obj.provider;
            if (provider.charAt(0) == "%") {
                provider = UrlDecode(provider).replace("&","&#38");
            }

            if(provider == "Limited Service") {
                provider=_("limited service");
            }
            else if (provider == "N/A") {
                provider=_("na");
            }
            return provider;
            }
        },
        {hdg: "roaming status", genHtml: (obj) => obj.roaming}
        ]},
    {
        members:[
        {hdg: "allowedBands", genHtml: (obj) => obj.selBand},
        {hdg: "currentBand", genHtml: (obj) => obj.currBand.replace("IMT2000", "WCDMA 2100")},
        {hdg: "status CScoverage", genHtml: (obj) => obj.coverage},
        ]}
    ],

    populate: function () {
        var obj = this.obj;
        var simStatus = obj.simStatus;
        var networkRegistration = obj.networkRegistration;
        if( simStatus!="SIM OK" ) {
            networkRegistration=9; //N/A
        }

        $("#networkRegistration").html(registrationStr[networkRegistration]);

        switch(parseInt(networkRegistration)) {
            case 0:
            case 2:
            case 9:
            obj.freq = _("na");
            break;
        }
        // roaming
        var roaming;
        var hint = obj.hint;
        if( isValidValue(hint)) {
            var patt=/%[0-1][0-9a-f]%/gi
            hint = hint.replace(patt, "%20%") // Some SIM cards have character less than %20. This ruins decoding rules.
            roaming = UrlDecode(hint);
        } else {
            roaming = "";
        }
        if(typeof(networkRegistration)!="undefined" && (networkRegistration==5)) {
            if(roaming!="")
                roaming = roaming+"&nbsp;&nbsp;-&nbsp;&nbsp;";
            roaming = roaming+"<font style='color:red'>"+_("roaming")+"</font>";
        } else {
            roaming = _("not roaming");
        }
        obj.roaming = roaming;

        //---- SIM Status
        var simStatusID: any =document.getElementById('simID');
        simStatusID.style.color = "RED";
        if( (simStatus.indexOf("SIM locked")!= -1)||(simStatus.indexOf("incorrect SIM")!= -1)||		(simStatus.indexOf("SIM PIN")!= -1) ) {
            simStatus = "SIM locked";
            simStatusID.style.color = 'Orange';
            $("#simID").html(_("detectingSIM")+"<i class='progress-sml'></i>");
        } else if( (simStatus.indexOf("CHV1")!= -1) || (simStatus.indexOf("PUK")!= -1) ) {
            pincounter=3;
            if (pukRetries != "0") {
                simStatus = "SIM PUK locked";
            } else {
                simStatus = "SIM invalidated";
            }
        } else if( (simStatus.indexOf("MEP")!= -1) ) {
            simStatus = "MEP locked";
            pincounter=3;
        }

        if(simStatus == "SIM OK") {
            simStatusID.style.color = 'GREEN';
            $("#simID").html(_("simOK")+"<i class='success-sml'></i>");
        } else if(simStatus == "N/A" || simStatus == "Negotiating") {
            obj.MCC=obj.MNC=obj.IMSI=obj.simICCID=_("na");
            if (pukRetries != "0") {
                if(obj.manualroamResetting=="1") {
                    simStatusID.style.color = "RED";
                    $("#simID").html(_("resettingModem")+"<i class='progress-sml'></i>");
                } else {
                    simStatusID.style.color = 'Orange';
                    $("#simID").html(_("detectingSIM")+"<i class='progress-sml'></i>");
                }
            } else {
                simStatusID.style.color = "RED";
                $("#simID").html(_("puk warningMsg1")+"<i class='warning-sml'></i>");
            }
        }
        else if(simStatus == "SIM not inserted" || simStatus == "SIM removed") {
            obj.MCC=obj.MNC=obj.IMSI=obj.simICCID=_("na");
            if (pukRetries != "0") {
            $("#simID").html(_("simIsNotInserted")+"<i class='warning-sml'></i>");
            }
            else {
            simStatusID.style.color = "RED";
            $("#simID").html(_("puk warningMsg1")+"<i class='warning-sml'></i>");
            }
        } else {
            if( prvSimStatus != simStatus ) {
                if( ++ pincounter > 1) {
                    prvSimStatus = simStatus;
                    simStatusID.style.color = "RED";
                    switch(simStatus) {
                        case "SIM removed":
                            blockUI_alert_l( _("admin warningMsg8"), function(){return;});
                            break;
                        case "SIM general failure":
                            $("#simID").html(simStatus);
                            blockUI_alert_l(_("admin warningMsg2"), function(){return;});
                        break;
                        case "SIM locked":
                            /* only when autopin is disabled, we redirect the page */
                            if(obj.autopin=="0") {
                                $("#simID").html(_("SIMlocked")+"<i class='warning-sml'></i>");
                                blockUI_alert_l(_("admin warningMsg3"), function(){window.location.href="/pinsettings.html";});
                            }
                            break;
                        case "SIM PUK locked":
                            $("#simID").html(_("status pukLocked")+"<i class='warning-sml'></i>");
                            blockUI_alert_l(_("admin warningMsg4"), function(){window.location.href="/pinsettings.html";});
                            break;
                        case "MEP locked":
                            $("#simID").html(simStatus);
                            blockUI_confirm(_("mep warningMsg2"), function(){window.location.href="/pinsettings.html?%4d%45%50%4c%6f%63%6b";});
                            break;
                        case "PH-NET PIN":
                        case "SIM PH-NET":
                            $("#simID").html(simStatus); // TODO - There was a window.location="/pinsettings.html" with session check;
                            break;
                        case "Network reject - Account":
                            $("#simID").html(simStatus);
                            blockUI_alert_l(_("admin warningMsg6"), function(){return;});
                            break;
                        case "Network reject":
                            $("#simID").html(simStatus);
                            blockUI_alert_l(_("admin warningMsg7"), function(){return;});
                            break;
                        case "SIM invalidated":
                            $("#simID").html(_("puk warningMsg1")+"<i class='warning-sml'></i>");
                            blockUI_alert_l(_("puk warningMsg2"), function(){return;});
                            break;
                        default:
                            $("#simID").html(simStatus);
                            pincounter = 0;
                            break;
                    }
                }
            }
        }
        //---- End of SIM Status

        _populateCols(this.columns, this.obj);
    },

    members: [
        hiddenVariable("networkRegistration", "wwan.0.system_network_status.reg_stat"),
        hiddenVariable("simStatus", "wwan.0.sim.status.status"),
        hiddenVariable("provider", "wwan.0.system_network_status.network"),
        hiddenVariable("connType", "wwan.0.system_network_status.service_type"),
        hiddenVariable("csq", "wwan.0.radio.information.signal_strength"),
        hiddenVariable("coverage", "wwan.0.system_network_status.system_mode"),
        hiddenVariable("hint", "wwan.0.system_network_status.hint.encoded"),
        hiddenVariable("simICCID", "wwan.0.system_network_status.simICCID"),
        hiddenVariable("manualroamResetting", "manualroam.resetting"),
        hiddenVariable("IMSI", "wwan.0.imsi.msin"),
        hiddenVariable("autopin", "wwan.0.sim.autopin"),
        hiddenVariable("operationMode", "wwan.0.PLMN_selectionMode"),
        hiddenVariable("selBand", "wwan.0.currentband.current_selband"),
        hiddenVariable("currBand", "wwan.0.system_network_status.current_band"),
    ]
}
);

stsPageObjects.push(cellularConnectionStatus);
