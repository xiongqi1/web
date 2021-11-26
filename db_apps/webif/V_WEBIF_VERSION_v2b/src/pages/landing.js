// Page that forces the user to change the preinstalled password.
//
// Copyright (C) 2018 NetComm Wireless Limited.

var factoryPasswordObject = editableTextVariable("factoryDefaultPassword", "factoryDefaultPassword") //.validateLua.push("root_password_correct(v)");
var names = ["webRootPassword", "webUserPassword", "sshRootPassword"];
var passwordObjects = names.map( function(name){ return editableStrongPasswordVariable(name, name).setVerify("verify password")});

class LandingInfoVariable extends ShownVariable {
  genHtml() {
    return htmlDiv({class:"p-des-full-width"}, htmlTag("p", {}, _("landingInfo")))
  }
}

var factPass = PageObj(
    "FactoryPassword",
    "factoryDefaultPassword",
{
  customLua: true,
  members: [
    factoryPasswordObject,
    new LandingInfoVariable("info", "")
  ]
});

var filename = "/opt/cdcs/upload/restore_cfg.tar.gz";

function onClickSubmit() {
  $.getJSON(
    "cgi-bin/restoreSettings.cgi?csrfTokenGet=" + csrfToken,
    {
       file:btoa(filename),
       pass:btoa(String($("#inp_pass").val())),
       factory:btoa(String($("#inp_factoryDefaultPassword").val()))
    },
    function(res) {
      var msg="";
      switch (res.cgiresult) {
        case   0:
          blockUI_wait(_("system warningMsg03")+".. "+_("GUI pleaseWait"));
          setTimeout("window.location.href='/index.html'", 80000);
        return;
        case 249: msg = "incorrectFactoryPassword"; break;
        case 250: msg = "Msg108"; break;
        case 251: msg = "system warningMsg09"; break;
        case 252: msg = "system warningMsg08"; break;
        case 253: msg = "system warningMsg07"; break;
        case 254: msg = "system warningMsg06"; break;
        default:  msg = "system warningMsg05";
      }
      $.blockUI({message:"<div>"+
        _(msg)+ "<div class='button-raw med'><button class='secondary med' onClick='$.unblockUI();'>"+
        _("CSok")+"</button></div></div>", css: {width:'300px'}});
  });
}

function startup() {
  $.blockUI( {message: _("welcomeMsg")+"<div class='button-raw med'>\
    <button class='secondary med' id='button1'>"+_("GUI next")+"</button></div>",
    css: {width:'320px', padding:'20px 20px'}
  });
  $('#button1').click( function() {
    actions();
    function actions() {
      $.unblockUI();
      $.blockUI( {message: _("entPasswdMsg")+"</br>\
        <div style='padding:12px 65px'><input class='large' type='text' id='pass'></div>\
        <div class='button-raw med' style='padding-top:40px'><button class='secondary med' id='button2'>"+_("GUI next")+"</button></div>",
        css: {width:'320px', padding:'20px 20px'}
      });
      $('#button2').click( function() {
        $.unblockUI();
        $('#inp_factoryDefaultPassword').val($('#pass').val());
        $.getJSON(
          "cgi-bin/restoreSettings.cgi?csrfTokenGet=" + csrfToken,
        {
          factory:btoa(String($("#inp_factoryDefaultPassword").val()))
        },
        function(res) {
          if (res.cgiresult != 255) {
            blockUI_wait(_("incorrectFactoryPassword"));
            setTimeout(actions, 2000);
          }
          else {
            $.blockUI( {message: _("selOptionsMsg")+"\
              <div style='padding:12px'><input type='radio' id='selOpt0' name ='selOpt' checked='checked' style='width:10px'>"+_("configNewMsg")+"</div>\
              <div style='padding:12px'><input type='radio' id='selOpt1' name ='selOpt' style='width:10px'>"+_("restoreBackupMsg")+"</div>\
              <div class='button-raw med' style='padding-top:10px'><button class='secondary med' id='button3'>"+_("CSok")+"</button></div>",
              css: {width:'320px', padding:'20px 20px'}
            });
            $('#button3').click (function() {
              $.unblockUI();
              $('#objouterwrapperFactoryPassword').hide();
              if( $("#selOpt0").is(":checked") ) {
                $('#objouterwrapperRestoreSettings').hide();
              }
              else {
                $('#objouterwrapperNewPasswords').hide();
                $('#inp_submit').removeClass('secondary');
                $('#buttonRow').hide();
              }
            });
          }
        });
      });
    }
  });
}

// uploading config file and restore system settings from it.
var restoreSettings = PageObj("RestoreSettings", "restoreSavedSettings",
{
   members: [
      new FileUploader("file","browse", filename, [".gz"], null, "Msg74", "Msg77"),
      editableTextVariable("pass", "filePassword",{required: false}),
      buttonAction("submit","Submit","onClickSubmit","")
   ]
});

var newPasswords = PageObj(
    "NewPasswords",
    "New Passwords",
    {
        customLua: true,
        members: passwordObjects
    }
);

var pageData : PageDef = {
#if defined V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "Factory State Landing",
  authenticatedOnly: false,
  validateOnload: false,
  menuPos: ["Login", ""],
  pageObjects: [factPass, restoreSettings, newPasswords],
  alertSuccessTxt: "factoryPasswordChangeSuccess",
  onReady: function (){
    $("#htmlGoesHere").append(genSaveRefreshCancel(true, false, false));
    startup();
    $("#saveButton").on('click', sendObjects);
  }
}

