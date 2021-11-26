var outDir = process.argv[2];
var jsFile = process.argv[3];
var processedJsFile = process.argv[4];
var outputExtension = process.argv[5];

function fileExists(fn) {
  try {
    fs.accessSync(fn);
    return true;
  } catch (e) {
    return false;
  }
}


function addExtraScript(script) {
  if (extraScripts.indexOf(script) < 0) {
    extraScripts.push(script);
  }
}


function genHtmlFile(pageScript) {

  var swVer = process.env.RELEASE;
  if(typeof swVer === "undefined") swVer = "X.X.X.X";

  // The v2 WebUI discards page titles by generating it dynamically in setmenu() so we will remove this for the moment
  var head = //htmlTag("title", {}, isDefine(pageData.title) ? pageData.title: "") +
        htmlTag("meta", {charset: "utf-8"}) +
        htmlTag("meta", {name: "viewport", content: "width=1100"}) +
        htmlTag("meta", {"http-equiv": "Cache-Control", content: "no-cache, no-store, must-revalidate"}) +
        htmlTag("meta", {"http-equiv": "format-detection", content: "telephone=no"});
  [
#ifdef V_WEBIF_SPEC_vdf
    '/vdf-lib/css/main.min.css',
#endif
    '/css/style.css',
    '/css/jquery-ui.css',
#ifndef V_WEBIF_SPEC_vdf
    '/css/validationEngine.jquery.css'
#endif
  ].forEach( function(styleFile){
    head += htmlTag("link", {rel: "stylesheet", href: styleFile + "?" + swVer})
  });

  // To reduce boilerplate in the page definition sources we
  // generate some common functions here
  var autoStubs = "";
  pageData.pageObjects.forEach(function(pgeObj){
    // Generate the page object visibility function if it was not already defined
    if (isDefined(pgeObj.visibilityVar)){
      var fnName = "onClick" + pgeObj.visibilityVar;
      if(eval("typeof " + fnName + " !== 'function'")){
        console.log("Create autostub function", fnName);
        autoStubs += "function "+fnName+"(toggleName, v) {" + pgeObj.objName + ".setVisible(v);}";
      }
    }
  });
#ifdef V_WEBIF_SERVER_turbo
  function rdbSubst(name) { return "'<<<" + name + ">>>'";}
#else
  function rdbSubst(name) { return "<%_val = get_single('" + name + "');%>'@@_val'";}
#endif
  // Appweb ESP is used three times below for
  // user, userlevel, and language.
  // If this framework is used on another webserver an alternate method needs to be used.
  head += htmlTag("script", {},
    // This first line is for frame breaking.
    // If this page is embedded in a frame it will reload into top
    "if (top !== self) top.location = self.location;"
    + "var nas_installed = " + rdbSubst('system.package.installed.nas') + ";"
    + "var ip_handover_en = " + rdbSubst('service.ip_handover.enable') + ";"
    + "var service_pppoe_server_0_enable = " + rdbSubst('service.pppoe.server.0.enable') + ";"
#ifdef V_ROUTER_TERMINATED_PPPOE
    + "var service_pppoe_server_0_wanipforward_enable = " + rdbSubst('service.pppoe.server.0.wanipforward_enable') + ";"
#endif
    + "var relUrlOfPage = '" + jsFile + ".html';"
#ifdef V_WEBIF_SERVER_turbo
    + "var user = 'root';" // TODO: implement user login
    + "var userlevel = 'root';"
#else
    + "var user = <%_val = session['user'];%>'@@_val';"
    + "var userlevel = <%_val = session['userlevel'];%>'@@_val';"
#endif
#ifdef V_WEBIF_SPEC_vdf
    + "var roam_simcard = " + rdbSubst('manualroam.custom_roam_simcard') + ";"
    + "var modules = [];"
#endif
    // The CSRF token
#ifdef V_WEBIF_SERVER_turbo
    + "var csrfToken = '{{csrfToken}}';"
#else
    + "var csrfToken = <%_val=generate_random_token(request['SESSION_ID']);"
        + "session['csrfToken_for'] = request['SCRIPT_NAME'];"
        + "session['csrfToken']=_val;%>'@@_val';"
#endif
    + autoStubs
  );

  var scriptFiles = [
    // This produces /js/lang/xlat.js
#ifdef V_WEBIF_SERVER_turbo
    "/js/{{lang}}/xlat.js",
#else
    "/js/<%_val = get_single('webinterface.language');if((_val=='')||(_val=='N/A'))_val='en';%>@@_val/xlat.js",
#endif
    "/js/jquery.min.js",
    "/js/jquery.validate.min.js",
#ifdef V_WEBIF_SPEC_vdf
    '/vdf-lib/js/main.min.js',
#else
    "/js/jquery.validationEngine.min.js",
#endif
    "/js/jquery.blockUI.min.js",
    "/js/jquery-ui.min.js",
    "/js/jquery.ui.touch-punch.min.js",
    "/js/NTC_UI.js",
    "/js/script.js",
#ifdef V_AUTO_POWERDOWN_y
    "/js/powerdown.js",
#endif
  ];

  scriptFiles = scriptFiles.concat(extraScripts);

  scriptFiles.forEach( function(scriptFile){
    head += htmlTag("script", {src: scriptFile + "?" + swVer}, "");
  });

  head += htmlTag("script", {}, "\n" + pageScript +
              // Kick off the data request as soon as we can as possible
              "if (pageData.disabled !== true) requestPageObjects();"
            );
#ifdef V_WEBIF_SERVER_turbo
  var authCheck = "{{#loggedIn}}"
  var authCheckEnd = "{{/loggedIn}}";
#else
  var authCheck = "<%if (" +
    (pageData.rootOnly === true ? "(session['userlevel']!='0')||":"") +
    "(request['SESSION_ID']!=session['sessionid']))" +
      "{redirect('/index.html?src='+request['SCRIPT_NAME']);exit(403);}%>"
  var authCheckEnd = "";
#endif

  var body = "";
  var html = (pageData.authenticatedOnly === false? "": authCheck) +
    "<!-- auto generated by genHtmlLua.js. -->" +
    "<!doctype html>" +
    htmlTag("html", {},
      htmlTag("head", {}, head) +
      htmlTag("body", {id: "body", style: "display: none;"}, body)
    )
    + (pageData.authenticatedOnly === false?"":authCheckEnd);

#ifdef V_WEBIF_SERVER_turbo
  var addUploader = false;
  for(var pageObj of pageData.pageObjects) {
    for(var obj of pageObj.members) {

      if(obj instanceof FileUploader) {
        addUploader = true;
        break;
      }

      if(addUploader) {
        break;
      }
    }
  }

  if (addUploader) {
    var src_file = "cgi-bin/variants/V_WEBIF_SPEC/lark/" + jsFile + ".lua";
    var dest_path = outDir + "/www/cgi-bin/";
    var dest_file = dest_path + jsFile + ".lua";

    try {
      if (!fs.existsSync(dest_path)){
        fs.mkdirSync(dest_path);
      }
      fs.copyFileSync(src_file, dest_file);
    }
    catch (error) {
      // TODO: terminate generation process
      console.error(error);
    }
  }
#else
  if (isDefined(filesUploadable)) {
    // Read the file upload template (Appweb specific)
    // and place the files required by the web page into it
    // The file name has '*' in it to represent parameters that are inserted into the file name
    // The upload code is a POST handler that prefixes the main html file
    var uploadCode = fs.readFileSync("./upload.esp",'utf8');
    var fnFileNameCode = [];
    for (var f in filesUploadable) {
      var fn = filesUploadable[f];
      for(var i = 0; fn.indexOf("*") >= 0;i++) {
        fn = fn.replace("*", "'+form['param" + i + "']+'");
      }
      fnFileNameCode.push("if(label=='"+f+"'){fn='"+fn+"';}");
    }
    uploadCode = uploadCode.replace("//fileNameGenerationCode", "\n"+fnFileNameCode.join("\n")+";\n");
    html = uploadCode + html;
  }
#endif

  fs.writeFileSync(outDir + "/www/" + jsFile + outputExtension, html);
}

if(isDefined(pageData.onDevice) && pageData.onDevice === false) {
  console.log(jsFile + " is not configured for this device");
}
else {
  pageData.URL = "/" + jsFile + ".html"; // This member is for the builder only
  genLuaScripts(pageData, outDir + "/www/cgi-bin/");

  // Read the page definition script file for inline inclusion into the html file
  genHtmlFile(fs.readFileSync(processedJsFile,'utf8'));
}
