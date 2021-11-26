var serverAddr = editableHostname("ServerAddr", "email smtp server addr");
var serverPort = editableBoundedInteger("ServerPort", "email smtp server port", 1, 65535, "Msg126", {helperText: " ( "+_("email smtp port desc")+" )"});
var serverSec = selectVariable("SecType", "encryption",
                  function(obj){return [["none","None"],["ssl","SSL/TLS"],["tls","STARTTLS"]];},
                  "onChangeEsec"
                );
function onChangeEsec(_this) {
#ifdef V_NON_SECURE_WARNING_y
  if (_this.value == "none") {
          blockUI_alert(_("emailNoneSecurityWarning"));
  }
#endif
}

var EmailClientCfg = PageObj("EmailClientCfg", "outgoing email settings",
{
  rdbPrefix: "service.email.client.conf.",
  members: [
    editableEmailAddress("FmAddr", "email from").setRdb("addr_fm"),
    editableEmailAddress("CcAddr", "email cc", {required: false}).setRdb("addr_cc"),
    serverAddr.setRdb("server_addr"),
    serverPort.setRdb("server_port").setSmall(),
    serverSec.setRdb("security"),
  ]
});

var userName = editableUserOrEmail("user", "user");
var userPassword = editablePasswordVariable("password", "password");
var authEnabled = objVisibilityVariable("UseAuth", "enable auth");
var EmailClientAuthCfg = PageObj("EmailClientAuthCfg", "",
{
  rdbPrefix: "service.email.client.conf.",
  members: [
    authEnabled.setRdb("useauth"),
    userName.setRdb("username"),
    userPassword.setRdb("password").setVerify("confirmPassword")
  ]
});

// Don't use built in validation for this as it may not be used
var testRecipient = editableTextVariable("recipient", "email test recipient");
testRecipient.validate = "";

function sendTestEmail() {
  var recipient = testRecipient.getVal();
  var commonHtmlTail = "<div class='button-raw med'><button class='secondary med' onClick='$.unblockUI();'>"
        + _("CSok") + "</button></div>";

  if (!validateEmail(recipient)){
    $.blockUI({message: htmlDiv({}, _("test email failed") + commonHtmlTail)});
    return;
  }

  var postParms = {
          recipient: recipient,
          server: serverAddr.getVal(),
          port: serverPort.getVal(),
          username: userName.getVal(),
          password: userPassword.getVal(),
          security: serverSec.getVal(),
          useauth: authEnabled.getVal()
      };

  $.post("cgi-bin/send_test_email.cgi?csrfTokenGet=" + csrfToken, postParms,
      function(res) {
          if (res.cgiresult == 1) {
              $.blockUI({message: "<div>"+_("test email succeeded") +commonHtmlTail});
          }
          else {
              $.blockUI({message: "<div>"+_("test email failed") +commonHtmlTail});
          }
      }
  );
}

var EmailClientTestCfg = PageObj("EmailClientTestCfg", "",
{
  readOnly: true,
  members: [
    testRecipient,
    buttonAction("testEmail", "send test email", "sendTestEmail")
  ]
});

var pageData : PageDef = {
#if defined V_SERVICES_UI_none || defined V_EMAIL_CLIENT_none
  onDevice : false,
#endif
  title: "Email Client Setting",
  menuPos: ["Services", "EMAIL_CLIENT"],
  pageObjects: [EmailClientCfg, EmailClientAuthCfg, EmailClientTestCfg],
  validateOnload: false,
  alertSuccessTxt: "email setting saved",
  onReady: function (){
    $("#objouterwrapperEmailClientTestCfg").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
  }
}
