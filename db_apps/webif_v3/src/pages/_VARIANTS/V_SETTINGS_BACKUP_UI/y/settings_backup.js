function onClickSaveBtnBackupCfg() {
  saveOrRestoreSettings("save", encode(passwordEdit.getVal()), csrfToken);
}

var passwordEdit = editablePasswordVariable("newPassword","password");
var BackupSettings = PageObj("BackupSettings", "Save a copy of current settings",
{
  members: [
    passwordEdit,
    editablePasswordVariable("confirmNewPassword", "confirm password")
      .setValidate(function (val) {
        return val === passwordEdit.getVal();},
        "The password and confirm password do not match."),
    buttonAction("save", "CSsave", "onClickSaveBtnBackupCfg();", "", {buttonStyle: "submitButton"})
  ],
});

// If web client was running on HTTPS previously then redirect to HTTPS
// regardless of current setting.
function redirectToHttpsAfterRestore(data)
{
  var newLocalHttps = parseInt(data.savedHttpsEn);
  var newLocalHttpsWebServer = parseInt(data.savedHttpsWebServerEn);
  return (newLocalHttps == 1 && newLocalHttpsWebServer == 1);
}

function rebootOnRestoreSettings(data) {
  // Cassini booting time is roughly 60 seconds at this time.
  // Additional time for power down & rebooting is 150 seconds including modem profile
  // restoring time.
  var estRebootingSecs = parseInt(RestoreSettings.obj.lastBootDuration) + 150;
  var savedLanIpAddr = '';
  var redirectProtocol = 'http';
  if (redirectToHttpsAfterRestore(data)) {
    redirectProtocol = 'https'
  }
  savedLanIpAddr = redirectProtocol + "://" + data.savedLanIpAddr;
  var currUrl = window.location.href.replace("/settings_backup.html", "");
  if (savedLanIpAddr == currUrl) {
    savedLanIpAddr = '';
  }
  restoreBtn.setEnable(false);
  sendSingleObject(RestoreSettings)
  blockUI_RebootWaiting();
  waitUntilRebootComplete(estRebootingSecs, savedLanIpAddr);
}

function onClickRestoreBtnRestoreCfg() {
  blockUI_confirm("It may take a few minutes to restore settings and reboot your device. Are you sure you want to continue?", function(obj){
    saveOrRestoreSettings("restore", encode(restorePasswordEdit.getVal()), csrfToken, rebootOnRestoreSettings);
    });
}

var memberName = "configFileUploader";
var labelText = "Select file to upload";
// This is where the uploaded file will be stored in device
var targetFile = "/usrdata/cache/settingsBackup.zip";
var restoreBtn = buttonAction("restore", "restore", "onClickRestoreBtnRestoreCfg", "", {buttonStyle: "submitButton"});
var fileExt = [".zip"];
var errExt = "Wrong file type. Files must have .zip extension. Please try again.";
var errLoad = "File upload failed. Please check the connection to the device and try again.";
var configFileUploader = fileUploader(
  memberName,
  labelText,
  targetFile,
  fileExt,
  null,
  errExt,
  errLoad,
  false,
  {
    onPosted: (resp) => {
      if (resp.message == "uploaded" && resp.result == "0") {
        restoreBtn.setEnable(true);
      }
    },
    onLengthyOperation: (respObj) => {}
  }
);

var restorePasswordEdit = editablePasswordVariable("restorePassword","password");

var RestoreSettings = PageObj("RestoreSettings", "Restore saved settings",
{
#ifdef COMPILE_WEBUI
  customLua: {
    set: (arr) => [...arr, `
      luardb.set("service.system.reset_reason", "Webui Request")
      luardb.set("service.system.reset", "1")
    `]
  },
#endif
  members: [
    configFileUploader,
    restorePasswordEdit,
    restoreBtn,
    hiddenVariable("lastBootDuration", "system.startup_duration").setReadOnly(true),
  ],
});

var pageData : PageDef = {
  title: "Settings backup and restore",
  menuPos: ["System", "SystemConfig", "SettingsBackup"],
  pageObjects: [BackupSettings, RestoreSettings],
  onReady: () => {
    restoreBtn.setEnable(false);
  },
};
