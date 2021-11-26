// Copyright (C) 2019 Casa Systems.

let g_powerdown_warning = false;

// executed when document is ready
$(setInterval(()=>powerdownCheck(), 10*1000));

function powerdownCheck() {
    var cgiUrl = '/cgi-bin/jsonSrvr.lua';
    $.getJSON(cgiUrl, "req=" + JSON.stringify({getObject: ["autoPowerdownSettings"], csrfToken: csrfToken})
    ).done((response)=>{
        if(0 != response.result) {
            return;
        }
        if(response.autoPowerdownSettings.powerdown_left != 0 &&
            window.location.pathname != "/index.html" && !g_powerdown_warning) {
            let duration = parseInt(response.autoPowerdownSettings.inactivity_duration);
            let left = response.autoPowerdownSettings.powerdown_left;

            let warning_time = 5 * 60;
            let half = duration / 2;

            if(half < warning_time) {
                warning_time = half;
            }

            if(left < warning_time) {
                powerdownWarn(duration);
            }
        }
        else if(g_powerdown_warning && response.autoPowerdownSettings.powerdown_left == 0) {
            $.unblockUI();
            g_powerdown_warning = false;
        }
    });
};

function powerdownWarn(duration) {
    g_powerdown_warning = true;
    blockUI_wait_confirm(_("nitToBeTurnedOff"), _("moreTime"), ()=>{
        var data = {
            setObject: [{
                inactivity_duration: duration.toString(),
                save: "",
                name: "autoPowerdownSettings"
            }],
            csrfToken: csrfToken
        };

        $.post("/cgi-bin/jsonSrvr.lua", "data=" + JSON.stringify(data)).done(()=>{
            g_powerdown_warning = false;
            $.unblockUI();
        });
    });
}
