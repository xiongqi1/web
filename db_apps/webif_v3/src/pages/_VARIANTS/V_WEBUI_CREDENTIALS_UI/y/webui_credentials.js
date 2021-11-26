var newPasswordEdit = editableStrongPasswordVariable("newPassword","New password")
            .setRequired(true);
var WebuiCredentials = PageObj("WebuiCredentials", "Changing Web UI credentials",
{
#ifdef COMPILE_WEBUI
    customLua: {
        lockRdb: false,
        get: function(arr) {
            arr.push("o.username = requestHandler.session.user");
            return arr;
        },
        validate: function(arr) {
            arr.push("local cp = decode(o.currentPassword) if #cp <= 0 or #cp > 64 then return false, 'oops! current password' end")
            arr.push("if o.confirmNewPassword ~= o.newPassword then return false, 'oops! confirm password' end");
            return arr;
        },
        set: function(arr) {
            arr.push("local cmd = string.format('/usr/bin/changing_webui_password.sh \"'..requestHandler.session.user..'\" \"%s\" \"%s\" >/dev/null 2>&1; echo $?', o.currentPassword, o.newPassword)");
            arr.push("local ret = executeCommand(cmd)");
            arr.push("local retNum = tonumber(ret[1])");
            arr.push("if retNum == 200 then return retNum,'The current password is incorrect.' else return retNum end");
            return arr;
        },
    },
#endif
    members: [
        staticTextVariable("username", "Username"),
        editablePasswordVariable("currentPassword", "Current password").setRequired(true)
            .setValidate(function (val) { return val.length > 0 && val.length <= 64; }, "Invalid password"),
        newPasswordEdit,
        editablePasswordVariable("confirmNewPassword", "Confirm new password").setRequired(true)
            .setValidate(function (val) { return val === newPasswordEdit.getVal(); }, "The password and confirm password do not match.")
    ],
});

var pageData : PageDef = {
    title: "Changing Web UI credentials",
    menuPos: ["System", "SystemConfig", "WebuiCredentials"],
    pageObjects: [WebuiCredentials],
    onReady: function () {
        appendButtons({"save":"CSsave"});
        setButtonEvent('save', 'click', sendObjects);
    }
};
