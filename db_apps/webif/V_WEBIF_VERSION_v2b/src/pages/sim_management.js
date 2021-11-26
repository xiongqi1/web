// sim_management.js
//
// WebUI page that is included in products with support for multiple SIMs.
// It lets the user specify how the router may switch between SIMs (and their networks)
// in response to changing conditions and events.  These situations include:
// - Removal/insertion of the external SIM
// - Loss of network
// - Denial of network
// - Network entering roaming mode
// - Loss of internet connectivity (as determined by LCP/ICMP pinging)
// - Degradation of signal strength
// - Using a specified quantity of data in a given time period
// - Sending a specified number of SMSes in a given time period
//
// All assessments and actions are done by the sim_management.lua process.  All control of the
// process is through RDB variables.  The update of settings is done atomically.  sim_management.lua
// only updates its local cache of settings when the RDB service.simmgmt.command value is set to
// update.
//
// Copyright (C) 2018 NetComm Wireless Limited.


// Build the name of a section based on the SIM number and the condition name.
function section_name(name, sim_num) {
    return  _("sim_switch_" + sim_num) + " " + _("sim_" + name)
}

// Construct the warning text we display if a given value is not in range.
function range_warning(lower, upper) {
    return _("value_must_be_between") + " " + lower.toString() + " " + _("and") + " " + upper.toString()
}

// This pages have a page object for each possible condition on each of the SIMs.
// This routine helps build those up.
function make_condition_page_object(sim_num, add_recheck_delay, name, extra_member_function) {
    var name_base = name + "_" + sim_num + "_";
    var common_members: PageElement[] = [
        objVisibilityVariable(name_base + "enabled", "enable").setRdb("enabled"),
        selectVariable(
            name_base + "weight", "condition_weight",
            function(obj){
                return [[1, "1 (weakest)"],[2, "2"],[3, "3"],[4, "4"],[5, "5 (critical)"]];
            }
        ).setRdb("weight")
    ];
    // on_sms_limit and on_data_limit fields don't use this.
    if (add_recheck_delay) {
        common_members = common_members.concat([
            editableBoundedInteger(
                name_base + "recheck_after",
                "sim_recheck_after",
                1, 120, range_warning(1, 120)
            ).setRdb("recheck_after_minutes")
        ])
    }
    var extra_members = extra_member_function(name_base);
    // The SMS and data usage limits use custom Lua code and so we flag this for their page objects.
    return PageObj(
        name + "_" + sim_num,
        section_name(name, sim_num),
        {
            customLua: !add_recheck_delay,
            rdbPrefix: "service.simmgmt." + sim_num + "." + name + ".",
            members: common_members.concat(extra_members)
        }
    );
}

// Data and SMS usage limits can be assessed over one of the following durations.
var period_types = ["month", "week", "day"];


// When the user changes the period type for a usual limit then we need the appropriate period start
// widget to be made visible and the others to be hidden.
function on_change_period_type(object) {
    var name_base = object.id.replace(/_period_type/, '').replace('sel_', '');
    for (var i = 0; i < 3; i++) {
        var period_type = period_types[i];
        var widget_handle = '#div_' + name_base + "_" + period_type + '_select';
        if (object.value == period_type) {
            setVisible(widget_handle,"1");
        } else {
            setVisible(widget_handle,"0");
        }
    }
}


// This is the set of widgets specific to a data usage or SMS usage condition.
// It consists of the period type selection and three period start selectors, of which only one is
// visible at any time.
// Returns handles to the 4 widgets in an array.
function period_selection_set(name_base) {
    // Build up the lists used in the selectVariable() widgets.
    var days = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
    var day_of_week_list = <OptionsType[]>days.map((day, index) => [index + 1, _(day)]);
    var hour_list = [];
    for (var i = 0; i < 10; i++) {
        hour_list.push([i, "0" + i.toString() + ":00"]);
    }
    for (var i = 10; i < 24; i++) {
        hour_list.push([i, i.toString() + ":00"]);
    }
    var day_of_month_list = [];
    for (var i = 1; i <= 28; i++) {
        switch (i) {
        case 1:
        case 21:
            day_of_month_list.push([i, i.toString() + "st"]);
            break;
        case 2:
        case 22:
            day_of_month_list.push([i, i.toString() + "nd"]);
            break;
        case 3:
        case 23:
            day_of_month_list.push([i, i.toString() + "rd"]);
            break;
        default:
            day_of_month_list.push([i, i.toString() + "th"]);
        }
    }
    var period_type_list = <OptionsType[]>period_types.map(pt => [pt, _(pt)]);

    // Create widgets.
    var period_type_obj = selectVariable(
        name_base + "period_type",
        "period_type",
        function(obj){return period_type_list;},
        "on_change_period_type"
    ).setRdb("period_type");

    var month_select_obj = selectVariable(
        name_base + "month_select",
        "day_of_month",
        function(obj){return day_of_month_list},
        "set_hidden_period_start"
    ).setIsVisible(function() {return period_type_obj.getVal() == "month";});

    var week_select_obj = selectVariable(
        name_base + "week_select",
        "day_of_week",
        function(obj){return day_of_week_list},
        "set_hidden_period_start"
    ).setIsVisible(function() {return period_type_obj.getVal() == "week";});

    var day_select_obj = selectVariable(
        name_base + "day_select",
        "hour_of_day",
        function(obj){return hour_list},
        "set_hidden_period_start"
    ).setIsVisible(function() {return period_type_obj.getVal() == "day";});

    return [
        period_type_obj,
        month_select_obj,
        week_select_obj,
        day_select_obj
    ];
}


// Add in widgets specific to SMS limit based control
function sms_extras(name_base) {
    var list: PageElement[] = [
        editableBoundedInteger(
            name_base + "max_count",
            "max_count",
            1, 10000, range_warning(1, 10000)
        ).setRdb("max_count")
    ];
    return list.concat(period_selection_set(name_base));
}


// Add in widgets specific to data limit based control
function data_extras(name_base) {
    var list: PageElement[] = [
        editableBoundedInteger(
            name_base + "max_count",
            "data_this_period_mb",
            1, 10000, range_warning(1, 10000)
        ).setRdb("max_count")
    ];
    return list.concat(period_selection_set(name_base));
}


// Add in widgets specific to "no network" based control
function duration_extras(name_base) {
    return [
        editableBoundedInteger(
            name_base + "duration_sec",
            "duration_sec",
            1, 3600, range_warning(1, 3600)
        ).setRdb("duration_sec")
    ];
}

// Add in widgets specific to weak signal based control
function weak_signal_extras(name_base) {
    return [
        editableBoundedInteger(
            name_base + "threshold_dbm_rsrp",
            "threshold_dbm_rsrp",
            -150, 0, range_warning(-150, 0)
        ).setRdb("threshold_dbm_rsrp"),
        editableBoundedInteger(
            name_base + "threshold_dbm_rssi",
            "threshold_dbm_rssi",
            -150, 0, range_warning(-150, 0)
        ).setRdb("threshold_dbm_rssi"),
        editableBoundedInteger(
            name_base + "duration_sec",
            "duration_sec",
            1, 3600, range_warning(1, 3600)
        ).setRdb("duration_sec"),
    ]
}


// Add in widgets specific to data connectivity based control
function ping_extras(name_base) {
    return [
        editableBoundedInteger(
            name_base + "check_interval_secs",
            "check_interval_secs",
            1, 3600, range_warning(1, 3600)
        ).setRdb("check_interval_secs"),
        editableBoundedInteger(
            name_base + "attempts",
            "ping_attempts",
            1, 20, range_warning(1, 20)
        ).setRdb("attempts"),
        editableBoundedInteger(
            name_base + "timeout_secs",
            "attempt_timeout_secs",
            1, 60, range_warning(1, 60)
        ).setRdb("timeout_secs"),
        selectVariable(
            name_base + "method", "ping_method",
            function(obj){return [["ICMP","ICMP"]];}
        ).setRdb("method"),
        editableIpAddressVariable(
            name_base + "host_addresses",
            "host_addresses"
        ).setRdb("host_addresses"),
    ]
}

// Used for fault types without extra conditions: network denial and roaming
function no_extras(name_base) {
    return []
}



// This replaces the auto stub because of the other objects it controls
function togglePermitSimFailover(toggleName, v) {
    // Ensure the master_failover_enable value itself is toggled.
    setToggle(toggleName, v);

    // Turn all the sections on or off.
    var conditions = [
        'on_sms_limit', 'on_roaming', 'on_data_limit', 'on_no_network',
        'on_network_denied',  'on_data_connection_fail', 'on_weak_signal'
    ];
    for (var sim_num = 1; sim_num <= 2; sim_num++) {
        for (var i = 0; i <  conditions.length; i++ ) {
            var name = "#objouterwrapper" + conditions[i] + "_" + sim_num;
            setVisible(name, v);
        }
    }
}


// The top widgets which are always displayed.
var Common = PageObj(
    "Common",
    "dualSimFailoverCommonElements",
    {
        rdbPrefix: "service.simmgmt.",
        members: [
            selectVariable(
                "primary", "primary_sim",
                function(obj){return [[1, "internal SIM"],[2, "removable SIM"]];}
            ).setRdb("primary"),
            toggleVariable(
                "switch_from_removed_sim",
                "switch_from_removed_sim"
            ).setRdb("switch_from_removed_sim"),
            toggleVariable(
                "master_failover_enable",
                "master_failover_enable",
                "togglePermitSimFailover"
            ).setRdb("master_failover_enable"),

        ]
    }
);


// This hidden variable results in RDB variable service.simmgmt.command being set to "update"
// whenever the save button is hit.  We place it in the TailEnd PageObj to ensure that all other
// RDB variables are set before the command one is.
class CommandVariable extends HiddenVariable {
    getVal() { return "update"; }
}

// A hidden page object that simply houses the commandVariable.
var TailEnd = PageObj(
    "TailEnd",
    "a",
    {
        rdbPrefix: "service.simmgmt.",
        members: [new CommandVariable("command", "command")]
    }
);



var on_no_network_1           = make_condition_page_object(1, true, 'on_no_network', duration_extras);
var on_network_denied_1       = make_condition_page_object(1, true, 'on_network_denied', no_extras);
var on_roaming_1              = make_condition_page_object(1, true, 'on_roaming', no_extras);
var on_weak_signal_1          = make_condition_page_object(1, true, 'on_weak_signal', weak_signal_extras);
var on_data_connection_fail_1 = make_condition_page_object(1, true, 'on_data_connection_fail', ping_extras);
var on_sms_limit_1            = make_condition_page_object(1, false, 'on_sms_limit', sms_extras);
var on_data_limit_1           = make_condition_page_object(1, false, 'on_data_limit', data_extras);

var on_no_network_2           = make_condition_page_object(2, true, 'on_no_network', duration_extras);
var on_network_denied_2       = make_condition_page_object(2, true, 'on_network_denied', no_extras);
var on_roaming_2              = make_condition_page_object(2, true, 'on_roaming', no_extras);
var on_weak_signal_2          = make_condition_page_object(2, true, 'on_weak_signal', weak_signal_extras);
var on_data_connection_fail_2 = make_condition_page_object(2, true, 'on_data_connection_fail', ping_extras);
var on_sms_limit_2            = make_condition_page_object(2, false, 'on_sms_limit', sms_extras);
var on_data_limit_2           = make_condition_page_object(2, false, 'on_data_limit', data_extras);

var page_objects = [
    Common,

    on_no_network_2,
    on_network_denied_2,
    on_roaming_2,
    on_weak_signal_2,
    on_data_connection_fail_2,
    on_sms_limit_2,
    on_data_limit_2,

    on_no_network_1,
    on_network_denied_1,
    on_roaming_1,
    on_weak_signal_1,
    on_data_connection_fail_1,
    on_sms_limit_1,
    on_data_limit_1,

    TailEnd
];


// This flag is set by the server (using the EXP code snippit) on page loading.
var sim_management_disabled = !toBool("<%get_single_direct('service.simmgmt.enabled');%>");

var pageData : PageDef = {
  title: "SIM Management",

#ifndef V_SIMMGMT_v2
  // This page only applies to product with the V2 SIM management.
  onDevice : false,
#endif

  // If RDB variable service.simmgmt.enabled is false (there's no internal SIM to switch to) then
  // this whole page is disabled and the user is merely presented with a message about the lack of
  // an alternate SIM.
  disabled: sim_management_disabled,
  alertHeading: "sim_management_disabled",
  alertText: "embedded_sim_not_fitted",

  menuPos: ["Internet", "SIMMGMT"],
  pageObjects: page_objects,
  validateOnload: true,
  alertSuccessTxt: "SIM management strategy saved",
  onReady: function () {
    $("#htmlGoesHere").after(genSaveRefreshCancel(true, false, false));
    $("#saveButton").on('click', sendObjects);
    // This needs to be called explicitly because of the other objects it controls
    togglePermitSimFailover("inp_master_failover_enable", Common.obj.master_failover_enable);
    setVisible('#objouterwrapperTailEnd', false);
  }
}
