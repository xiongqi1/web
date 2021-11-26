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
  #if defined(V_MEPLOCK)
          arr.push("if getRdb('meplock.status') == 'locked' then o.simStatus = 'MEP locked' end");
  #endif
          return arr;
        },
      },

      columns : [{
        members:[
          {hdg: "networkRegistrationStatus", name: "networkRegistration"},
          {hdg: "simStatus", name: "simID"}]
      },
      {
        members:[
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
          {hdg: "signal strength",
            genHtml: (obj) => {
              var csq = obj.csq;
              var connType = obj.connType
              csq = csq.substring(0, csq.indexOf("dBm") );
              var csqstr = _("not available");
              var imageidx;
              if(connType.substring(0, 3) == "GSM") {
                if(csq == 0)
                imageidx = 6;
                else if(csq >= -77 ) {
                csqstr = _("high");
                imageidx = 1;
                }
                else if(csq >= -91 ) {
                csqstr = _("medium");
                if(csq >= -85 )
                  imageidx = 2;
                else
                  imageidx = 3;
                }
                else if(csq >= -109 ) {
                csqstr = _("low");
                if(csq >= -101)
                  imageidx = 4;
                else
                  imageidx = 5;
                }
                else {
                imageidx = 6;
                }
              }
              else if(connType.substring(0, 3) == "LTE") {
                if(csq == 0)
                imageidx = 6;
                else if(csq > -70 ) {
                csqstr = _("high");
                imageidx = 1;
                }
                else if(csq > -90 ) {
                csqstr = _("medium");
                if(csq > -80 )
                  imageidx = 2;
                else
                  imageidx = 3;
                }
                else if(csq > -120 ) {
                csqstr = _("low");
                if(csq > -100)
                  imageidx = 4;
                else
                  imageidx = 5;
                }
                else {
                imageidx = 6;
                }
              }
              else {
                if(csq == 0)
                imageidx = 6;
                else if(csq >= -77 ) {
                csqstr = _("high");
                imageidx = 1;
                }
                else if(csq >= -91 ) {
                csqstr = _("medium");
                if(csq >= -85 )
                  imageidx = 2;
                else
                  imageidx = 3;
                }
                else if(csq >= -109 ) {
                csqstr = _("low");
                if(csq >= -101)
                  imageidx = 4;
                else
                  imageidx = 5;
                }
                else {
                  imageidx = 6;
                }
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
        }
          },
          {hdg: "status CSfrequency", genHtml: (obj) => obj.freq.replace("IMT2000", "WCDMA 2100")}
        ]
      },
      {
        members:[
          {hdg: "status CScoverage", genHtml: (obj) => obj.coverage},
          {hdg: "roaming status", genHtml: (obj) => obj.roaming}
        ]
      }],

      populate: function () {
        var obj = this.obj;
        var simStatus = obj.simStatus;
        var networkRegistration = obj.networkRegistration;
  #ifdef V_MANUAL_ROAMING_vdfglobal
        if( simStatus!="SIM OK" && networkRegistration=="N/A" ) {
          networkRegistration=9; //N/A
        }
  #else
        if( simStatus!="SIM OK" ) {
          networkRegistration=9; //N/A
        }
  #endif

  #ifdef V_MANUAL_ROAMING_vdfglobal
        var roaming_algorithm_running;
  /*
        if (mrs_cm_sus == "N/A" || mrs_cm_sus == "")
          mrs_cm_sus = 0

        if(uptime < mrs_cm_sus)
          networkRegistration=2;
  */
        roaming_algorithm_running=simStatus=="SIM OK" && networkRegistration!=1 && networkRegistration!=5;

        /* roaming registration */
        var reg_msg="";
        if( roaming_algorithm_running && isValidValue(obj.mrs_msg) ) {
          var msgs=obj.mrs_msg.split(/,/);
          if(msgs.length>1 && msgs[0]!=99) {

            /* roaming status messages */
            var reg_msgs=[
              _("manual roaming status 0"),
              _("manual roaming status 1"),
              _("manual roaming status 2"),
              _("manual roaming status 3"),
              _("manual roaming status 4"),
              _("manual roaming status 5"),
              _("manual roaming status 6"),
              _("manual roaming status 7"),
              _("manual roaming status 8"),
              null, // 9
              null, // 10
              null, // 11
              _("manual roaming status 12"), // 12
              _("manual roaming status 13"), // 13
              null, // 14
              null, // 15
              null, // 16
              null, // 17
              null, // 18
              null, // 19
            ];

            /* get tokens */
            var token_nw=obj.mrs_nw!="N/A"?obj.mrs_nw:"";
            var token_rssi=token_rssi!="N/A"?obj.mrs_rssi:"";
            var token_dmin=parseInt(obj.mrs_dmin) || 0;
            var token_start=parseInt(obj.mrs_start) || 0;
            var token_now=parseInt(obj.mrs_now) || 0;
            var token_left=(token_dmin+token_start)-token_now;

            /* get mm:ss format */
            token_left=(token_left<0)?0:token_left;
            var token_mm_ss=pad((Math.floor(token_left/60)).toString(),2) + ":" + pad((token_left%60).toString(),2);

            /* get reg msg */
            var reg_msg_idx=msgs[0];

            /* use a different message when token left is zero */
            if(!token_left && (reg_msg_idx==2 || reg_msg_idx==3)) {
              reg_msg_idx=parseInt(reg_msg_idx)+10;
            }
            reg_msg=reg_msgs[reg_msg_idx];

            /* replace keys */
            reg_msg=reg_msg.replace("<provider>",token_nw);
            reg_msg=reg_msg.replace("<rssi>",token_rssi);
            reg_msg=reg_msg.replace("<reboot_mm_ss>",token_mm_ss);
          }
        }

        /* print registration status - print reg_msg if existing */
        $("#networkRegistration").html(reg_msg!=""?reg_msg:registrationStr[networkRegistration]);

        // do not show CSQ when not registered
        if(networkRegistration == 0) {
          obj.csq = 0;
        }

  #else // V_MANUAL_ROAMING_vdfglobal
        $("#networkRegistration").html(registrationStr[networkRegistration]);
  #endif // V_MANUAL_ROAMING_vdfglobal

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
        }
        else {
          roaming = "";
        }
        if(typeof(networkRegistration)!="undefined" && (networkRegistration==5)) {
          if(roaming!="")
            roaming = roaming+"&nbsp;&nbsp;-&nbsp;&nbsp;";
          roaming = roaming+"<font style='color:red'>"+_("roaming")+"</font>";
        }
        else {
          roaming = _("not roaming");
        }
        obj.roaming = roaming;

        //---- SIM Status
        var simStatusID: any =document.getElementById('simID');
        simStatusID.style.color = "RED";
        if( (simStatus.indexOf("SIM locked")!= -1)||(simStatus.indexOf("incorrect SIM")!= -1)||(simStatus.indexOf("SIM PIN")!= -1) ) {
          simStatus = "SIM locked";
          simStatusID.style.color = 'Orange';
          $("#simID").html(_("detectingSIM")+"<i class='progress-sml'></i>");
        }
        else if( (simStatus.indexOf("CHV1")!= -1) || (simStatus.indexOf("PUK")!= -1) ) {
          pincounter=3;
          if (pukRetries != "0") {
            simStatus = "SIM PUK locked";
          }
          else {
            simStatus = "SIM invalidated";
          }
        }
        else if( (simStatus.indexOf("MEP")!= -1) ) {
          simStatus = "MEP locked";
          pincounter=3;
        }

        if(simStatus == "SIM OK") {
          simStatusID.style.color = 'GREEN';
          $("#simID").html(_("simOK")+"<i class='success-sml'></i>");
        }
        else if(simStatus == "N/A" || simStatus == "Negotiating") {
          obj.MCC=obj.MNC=obj.IMSI=simICCID=_("na");
          if (pukRetries != "0") {
            if(manualroamResetting=="1") {
              simStatusID.style.color = "RED";
              $("#simID").html(_("resettingModem")+"<i class='progress-sml'></i>");
            }
            else {
              simStatusID.style.color = 'Orange';
              $("#simID").html(_("detectingSIM")+"<i class='progress-sml'></i>");
            }
          }
          else {
            simStatusID.style.color = "RED";
            $("#simID").html(_("puk warningMsg1")+"<i class='warning-sml'></i>");
          }
        }
        else if(simStatus == "SIM not inserted" || simStatus == "SIM removed") {
          obj.MCC=obj.MNC=IMSI=simICCID=_("na");
          if (pukRetries != "0") {
            $("#simID").html(_("simIsNotInserted")+"<i class='warning-sml'></i>");
          }
          else {
            simStatusID.style.color = "RED";
            $("#simID").html(_("puk warningMsg1")+"<i class='warning-sml'></i>");
          }
        }
        else {
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
                // Beside autopin, extra conditional check is to fix an issue where after the device remembers PIN of any SIM, the status page does not prompt user to unlock SIM
                // if user inserts another SIM or the same SIM of which PIN has been changed.
                // This fix depends on wwan_pin and wwan_pin_ccid which are saved after SIM verification is successful by simple_at_manager or qmimgr.
                // It is applied for wwan.0.if == "atqmi" or "at" because cnsmgr also unlocks SIM but does not process wwan_pin and wwan_pin_cci
                // while qmimgr does not process autopin.
                // wwan.0.if "atcns" may work because /usr/bin/cdcs_init_wwan_pm launches simple_at_manager with "-t all -f allcns" i.e including "simcard" feature, but it should be tested.
                if(autopin=="0" || ((module_if_type == "atqmi" || module_if_type == "at") && ((currentICCID != "N/A" && rememberedICCID != currentICCID) || rememberedICCID == "N/A"))) {
                  $("#simID").html(_("SIMlocked")+"<i class='warning-sml'></i>");
                  blockUI_alert_l(_("admin warningMsg3"), function(){window.location="/pinsettings.html";});
                }
                break;
              case "SIM PUK locked":
                $("#simID").html(_("status pukLocked")+"<i class='warning-sml'></i>");
                blockUI_alert_l(_("admin warningMsg4"), function(){window.location="/pinsettings.html";});
                break;
              case "MEP locked":
                $("#simID").html(simStatus);
                blockUI_confirm(_("mep warningMsg2"), function(){window.location="/pinsettings.html?%4d%45%50%4c%6f%63%6b";});
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
        hiddenVariable("networkRegistrationRejectCause", "wwan.0.system_network_status.rej_cause"),
        hiddenVariable("simStatus", "wwan.0.sim.status.status"),
  #ifdef V_MANUAL_ROAMING_vdfglobal
  /*      clock_t now;
        struct tms tmsbuf;
        now=times(&tmsbuf)/sysconf(_SC_CLK_TCK);
  */
        /* roaming information - registration status */
        hiddenVariable("mrs_msg", "manualroam.stat.msg"),
        hiddenVariable("mrs_nw", "manualroam.stat.network"),
        hiddenVariable("mrs_rssi", "manualroam.stat.rssi"),
        hiddenVariable("mrs_dmin", "manualroam.stat.delay_min"),
        hiddenVariable("mrs_start", "manualroam.stat.delay_start"),
  //      hiddenVariable("mrs_now='%ld';", now);
        hiddenVariable("mrs_cm_sus", "manualroam.suspend_connection_mgr"),
        hiddenVariable("mrs_custom_roam_simcard", "manualroam.custom_roam_simcard"),
  #endif
        hiddenVariable("provider", "wwan.0.system_network_status.network"),
        hiddenVariable("freq", "wwan.0.system_network_status.current_band"),
        hiddenVariable("connType", "wwan.0.system_network_status.service_type"),
        hiddenVariable("csq", "wwan.0.radio.information.signal_strength"),
        hiddenVariable("coverage", "wwan.0.system_network_status.system_mode"),
        hiddenVariable("hint", "wwan.0.system_network_status.hint.encoded")
      ]
  }
);

stsPageObjects.push(cellularConnectionStatus);
