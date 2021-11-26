// Mechanism to require the user re-enter the root password when they attempt to change sensitive fields
// such as password change inputs and the hardware reset button.  HTML items are marked for this
// protection by adding them to class "password-locked".  apply_password_lock_to_inputs() is called
// in the $(document).ready() function body.  When focus changes to such an item then a blocking
// dialog box appears challenging the user for the root password.  If the user enters the valid
// password then the dialog disappears and the item can be edited.  Any other password-locked item
// on this page would also be unlocked.
//
// Copyright (C) 2018 NetComm Wireless Limited.

// Called on HTML load completion, adjust any HTML item of the "password-locked" class so that they
// can't be edited until the user has been successfully challenged for the root password.

function apply_password_lock_to_inputs(csrf_token_value)
{

    // Set false on a successful answer to the challenge.
    var items_locked = true;


    // A password locked item has received focus, construct the dialog box, send the offered
    // password to the server and, if it valid, then unlock all the locked items.
    function challenge()
    {
        // The <input> element in the dialog.
        var password_field;
        // So we can return focus after we close the dialog.
        var item_to_focus = this;

        if (!items_locked) {
            // Nothing to do.
            return;
        }

        // Callback when the user clicks okay in the dialog.  Sends the offered password to the
        // server for validation (by check_password.cgi).
        function check_password()
        {
            var password = password_field.val();
            if (password == '') {
                // No password: keep dialog box up.
                return;
            }

            // Send the password off, if it is valid the we get a "password_state: good" back.
            // $.post(cgiUrl, setObject: objs, csrfToken: csrfToken}), null, "json")
            $.post(
                '/cgi-bin/jsonSrvr.lua',
                JSON.stringify({
                    setObject: [{
                        name:"PasswordValidate",
                        password: window.btoa(password)
                    }],
                    csrfToken: csrf_token_value
                }),
                null,
                "json"
            ).done(function(response) {
                junk = response;  //TEMP!!!
                if (response.result == 0) {
                    if (response.PasswordValidate.state == 'good') {
                        // Unlock items on this page.
                        items_locked = false;
                        $(".password-locked").prop('readonly', false);

                        // Close the dialog and return focus to the appropriate element.
                        $.unblockUI();
                        item_to_focus.focus();
                        // Give it a click, just in case it was a button.
                        item_to_focus.click();
                        return;
                    } else if (response.PasswordValidate.state == 'bad') {
                        // Report a bad password in a new dialog box and return.
                        $.unblockUI();
                        blockUI_alert(_('incorrectPassword'));
                        return;
                    }
                }
                // Either the reply status is not "success" or the password_state is not good
                // or bad.  Report this and return without action.
                $.unblockUI();
                blockUI_alert(_('httpreq err'));

            });
        }

        // If the user cancels then close the dialog without action.
        function cancel()
        {
            $.unblockUI();
        }

        // Construct the dialog box: instructions, password entry, two buttons.
        explanation_div = $('<div>', {text: _("enterRootPasswordToConfirmAction")});

        ok_button = $('<button>', {class:'secondary med', text:_("CSok"), click: check_password});
        cancel_button = $('<button>', {class:'secondary med', text: _("cancel"), click: cancel});
        button_div = $('<div>', {class: 'button-double'}).append(ok_button, cancel_button);

        // 95% seems to be needed to centre correctly.
        password_field = $('<input>', {type:'password', style:'width: 95%;'});
        password_div = $('<div>').append(password_field);

        challenge_box = $('<div>').append(explanation_div, password_div, button_div);

        $.blockUI({
            message: challenge_box,
            // These hardcoded dimensions copied from utils.js
            css: {width:'380px', padding:'20px 20px'}
        });
    }

    // Prevent the element from responding to a click if the lock is in effect.  This is needed for
    // locking radio buttons.
    function ignore_if_locked()
    {
        return !items_locked;
    }

    // All "password-locked" items are made read-only and will call challenge() when they get focus.
    $(".password-locked").focus(challenge).prop('readonly', true);
    $(".password-locked").children().focus(challenge);

    // Additionally, clicking on radio buttons has not effect if they are locked.
    $(".password-locked input:radio").click(ignore_if_locked);
}

