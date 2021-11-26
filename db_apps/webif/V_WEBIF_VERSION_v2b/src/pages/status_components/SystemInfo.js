function is_valid_model_name(model)
{
	// no signle letter phone module name!
	if(model.length < 2)
		return false;

	if(model == "N/A")
    return false;

	// should start with a letter!
	if(!isLetter(model.charAt(0)))
    return false;

	/* from this point, we need a human in the routine! */

	// not model name - many ZTE module has this incorrect product name in USB product configuration
	// ex) ZTEConfiguration, ZTEWCDMATechnologiesMSM, ZTECDMATechnologiesMSM
	if(model.startsWith("ZTE") && model.indexOf("MSM") >= 0)
    return false;
	if(model.indexOf("Configuration") >= 0)
    return false;

	// Sierra USB-306 has this incorrect product name
	if(model.toLowerCase() == "HSPA Modem".toLowerCase())
    return false;

	if(model.toLowerCase() == "HUAWEI Mobile".toLowerCase())
    return false;

	return true;
}

function trim_quotatons(str) {
  var start = str.charAt(0) == '"'? 1: 0;
  var end = str.charAt(str.length-1) == '"'? str.length-1: str.length;
  return str.substring(start,end);
}

var SystemInfo = PageObj("StsSystemInfo", "sysInfo",
  {
    customLua: {
      lockRdb: false,
      get: function(arr) {
        arr.push("for line in io.lines('/etc/version.txt') do o.version = line break end");
        return arr;
      },
    },

    readOnly: true,
    column: 1,
    genObjHtml: genCols,
    pollPeriod: 1000,

    columns : [{
      heading:"status system up time",
      members:[{hdg: "dt"}]
    },
    {
      heading:"routerVersion",
      members:[
        {hdg: "boardVersion", genHtml: (obj) => obj.hw_ver === "" ? "1.0" : obj.hw_ver},
        {hdg: "routerSerialNumber", genHtml: (obj) => obj.serialnum, isVisible: (obj) => isValidValue(obj.serialnum)},
        {hdg: "firmware", genHtml: (obj) => isDefined(obj.version) ? "V" + obj.version : "XXX"},
        {hdg: "hardwareVersion",
          genHtml: (obj) => {
                  if (obj.model == "vdf_nwl10")
                    return "MachineLink 3G";
                  if (obj.model == "vdf_nwl22" || obj.model == "vdf_nwl22w")
                    return _("machineLink4g");
                  return _("machineLink3gPlus");
              }

        }
      ]
    },
    {
      heading:"advStatus phoneModule",
      members:[
        {hdg: "model",
          genHtml: (obj) => {
            var model_name = "";
            if(obj.proto_type.indexOf("at") >= 0) {
              // take AT model
              obj.at_model = trim_quotatons(obj.at_model);
              if(is_valid_model_name(obj.at_model))
                model_name=obj.at_model;
            }

            // take the cfg model name - we override any module for a certain variant
            if(!is_valid_model_name(model_name) && is_valid_model_name(obj.cfg_model))
              model_name=obj.cfg_model;
            // take udev configurated name
            if(!is_valid_model_name(model_name) && is_valid_model_name(obj.udev_cfg))
              model_name=obj.udev_cfg;
            // take usb configuraiton product
            if(!is_valid_model_name(model_name) && is_valid_model_name(obj.udev_model))
              model_name=obj.udev_model;
            // do not take if it is invalid
            if(!is_valid_model_name(model_name))
              model_name="N/A";
            return model_name;
          }
        },
        {hdg: "module firmware",
          genHtml: (obj) => {
            var moduleFirmwareVersion = obj.moduleFirmwareVersion;
            if (obj.module_type == 'sierra')
              moduleFirmwareVersion = moduleFirmwareVersion.substr(moduleFirmwareVersion.indexOf("_")+1,11);
            if (obj.module_type == '')
              moduleFirmwareVersion = '';
#if defined(V_MODULE_MC7304)
            //temporary patch for unofficial Sierra module firmware
            if (moduleFirmwareVersion=="00.00.00.00") {
              moduleFirmwareVersion = "VF GTM Ver. 1.0";
            }
#endif
            return moduleFirmwareVersion;
          }
        },
        {hdg: "cid",
          genHtml: function(obj) {
            $("#" + this.name).css("display", "inline-block").css("word-wrap","break-word").css("width","180px");
            return obj.moduleFirmwareVerExt;
          },
          isVisible: (obj) => isValidValue(obj.moduleFirmwareVerExt)
        },
        {hdg: "status CSimei", genHtml: (obj) => obj.imei}
      ]
    }],

    populate: populateCols,

    members: [
#ifdef PLATFORM_Serpent
      hiddenVariable("hw_ver", "system.product.hwver"),
      hiddenVariable("serialnum", "system.product.sn"),
#else
      hiddenVariable("hw_ver", "uboot.hw_ver"),
      hiddenVariable("serialnum", "uboot.sn"),
#endif
      hiddenVariable("model", "system.product.model"),
      hiddenVariable("cfg_model", "webinterface.module_model"),   // hard-coded name from rdb configuration
      hiddenVariable("at_model", "wwan.0.model"),                 // result from AT command (AT+CGMM)
      hiddenVariable("udev_cfg", "wwan.0.module_name"),           // vendor-device-id-based hard-coded name
      hiddenVariable("udev_model", "wwan.0.product_udev"),        // USB configuraiton - product
      hiddenVariable("proto_type", "wwan.0.if"),
      hiddenVariable("moduleFirmwareVersion", "wwan.0.firmware_version"),
      hiddenVariable("module_type", "wwan.0.module_type"),
      hiddenVariable("moduleFirmwareVerExt", "wwan.0.firmware_version_cid"),
      hiddenVariable("imei", "wwan.0.imei")
    ]
});

stsPageObjects.push(SystemInfo);
