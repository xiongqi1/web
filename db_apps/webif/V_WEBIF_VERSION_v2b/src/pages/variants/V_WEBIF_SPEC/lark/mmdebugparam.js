//Copyright (C) 2019 NetComm Wireless Limited.
var sas_grants = [];

for(var i=1; i<=15; i++) {
  var obj_id = "sas_grant_state_" + i;
  var obj_name = "SAS grant " + i + " state";
  var obj_rdb ="sas.grant." + i + ".state";
  sas_grants.push(staticTextVariable(obj_id, obj_name).setRdb(obj_rdb));

  obj_id = "sas_grant_reason_" + i;
  obj_name = "SAS grant " + i + " reason";
  obj_rdb ="sas.grant." + i + ".reason";
  sas_grants.push(staticTextVariable(obj_id, obj_name).setRdb(obj_rdb));

  obj_id = "sas_grant_next_heartbeat_" + i;
  obj_name = "SAS grant " + i + " next heartbeat";
  obj_rdb ="sas.grant." + i + ".next_heartbeat";
  sas_grants.push(staticTextVariable(obj_id, obj_name).setRdb(obj_rdb));
 }

var po = PageObj("debugParameters", "Debug Parameters",
{
  readOnly: true,
  members: [
    staticTextVariable("wwan_0_sim_status_status", "WWAN 0 SIM status").setRdb("wwan.0.sim.status.status"),
    staticTextVariable("wwan_0_sim_data_msisdn", "WWAN 0 data MSISDN").setRdb("wwan.0.sim.data.msisdn"),
    staticTextVariable("wwan_0_imsi_msin", "WWAN 0 IMSI MSIN").setRdb("wwan.0.imsi.msin"),
    staticTextVariable("wwan_0_manual_cell_meas_ecgi", "WWAN 0 cell ECGI").setRdb("wwan.0.manual_cell_meas.ecgi"),
    staticTextVariable("wwan_0_signal_0_rsrp", "WWAN 0 RSRP").setRdb("wwan.0.signal.0.rsrp"),
    staticTextVariable("wwan_0_signal_rsrq", "WWAN 0 RSRQ").setRdb("wwan.0.signal.rsrq"),
    staticTextVariable("wwan_0_signal_rssi", "WWAN 0 RSSI").setRdb("wwan.0.signal.rssi"),
    staticTextVariable("wwan_0_signal_rssinr", "WWAN 0 RSSINR").setRdb("wwan.0.signal.rssinr"),
    staticTextVariable("wwan_0_signal_snr", "WWAN 0 SNR").setRdb("wwan.0.signal.snr"),
    staticTextVariable("wwan_0_plmn_plmn_list_1_status", "WWAN 0 PLMN list 1 status").setRdb("wwan.0.plmn.plmn_list.1.status"),

    staticTextVariable("wwan_0_system_network_status_CellID", "WWAN 0 Cell ID").setRdb("wwan.0.system_network_status.CellID"),
    staticTextVariable("wwan_0_system_network_status_ECGI", "WWAN 0 ECGI").setRdb("wwan.0.system_network_status.ECGI"),
    staticTextVariable("wwan_0_system_network_status_PLMN", "WWAN 0 PLMN").setRdb("wwan.0.system_network_status.PLMN"),
    staticTextVariable("wwan_0_system_network_status_attached", "WWAN 0 attached").setRdb("wwan.0.system_network_status.attached"),
    staticTextVariable("wwan_0_system_network_status_channel", "WWAN 0 channel").setRdb("wwan.0.system_network_status.channel"),
    staticTextVariable("wwan_0_system_network_status_current_band", "WWAN 0 current band").setRdb("wwan.0.system_network_status.current_band"),
    staticTextVariable("wwan_0_system_network_status_eNB_ID", "WWAN 0 eNB ID").setRdb("wwan.0.system_network_status.eNB_ID"),
    staticTextVariable("wwan_0_system_network_status_eci_pci_earfcn", "WWAN ECI PCI EARFCN").setRdb("wwan.0.system_network_status.eci_pci_earfcn"),
    staticTextVariable("wwan_0_system_network_status_registered", "WWAN 0 registered").setRdb("wwan.0.system_network_status.registered"),
    staticTextVariable("wwan_0_system_network_status_service_type", "WWAN 0 service type").setRdb("wwan.0.system_network_status.service_type"),
    staticTextVariable("wwan_0_system_network_status_simICCID", "WWAN 0 SIM ICCID").setRdb("wwan.0.system_network_status.simICCID"),
    staticTextVariable("wwan_0_system_network_status_system_mode", "WWAN 0 system mode").setRdb("wwan.0.system_network_status.system_mode"),

    staticTextVariable("link_profile_1_apn", "Link 1 APN").setRdb("link.profile.1.apn"),
    staticTextVariable("link_profile_1_connect_progress", "Link 1 connect progress").setRdb("link.profile.1.connect_progress"),
    staticTextVariable("link_profile_1_dns1", "Link 1 DNS1").setRdb("link.profile.1.dns1"),
    staticTextVariable("link_profile_1_dns2", "Link 1 DNS2").setRdb("link.profile.1.dns2"),
    staticTextVariable("link_profile_1_enable", "Link 1 enabled").setRdb("link.profile.1.enable"),
    staticTextVariable("link_profile_1_gw", "Link 1 gateway").setRdb("link.profile.1.gw"),
    staticTextVariable("link_profile_1_interface", "Link 1 interface").setRdb("link.profile.1.interface"),
    staticTextVariable("link_profile_1_iplocal", "Link 1 local IP").setRdb("link.profile.1.iplocal"),
    staticTextVariable("link_profile_1_ipv6_gw", "Link 1 IPV6 gateway").setRdb("link.profile.1.ipv6_gw"),
    staticTextVariable("link_profile_1_ipv6_ipaddr", "Link 1 IPV6 IP address").setRdb("link.profile.1.ipv6_ipaddr"),
    staticTextVariable("link_profile_1_status_ipv4", "Link 1 IPV4 status").setRdb("link.profile.1.status_ipv4"),
    staticTextVariable("link_profile_1_status_ipv6", "Link 1 IPV6 status").setRdb("link.profile.1.status_ipv6"),

    staticTextVariable("link_profile_2_apn", "Link 2 APN").setRdb("link.profile.2.apn"),
    staticTextVariable("link_profile_2_connect_progress", "Link 2 connect progress").setRdb("link.profile.2.connect_progress"),
    staticTextVariable("link_profile_2_dns1", "Link 2 DNS1").setRdb("link.profile.2.dns1"),
    staticTextVariable("link_profile_2_dns2", "Link 2 DNS2").setRdb("link.profile.2.dns2"),
    staticTextVariable("link_profile_2_enable", "Link 2 enabled").setRdb("link.profile.2.enable"),
    staticTextVariable("link_profile_2_gw", "Link 2 gateway").setRdb("link.profile.2.gw"),
    staticTextVariable("link_profile_2_interface", "Link 2 interface").setRdb("link.profile.2.interface"),
    staticTextVariable("link_profile_2_iplocal", "Link 2 local IP").setRdb("link.profile.2.iplocal"),
    staticTextVariable("link_profile_2_ipv6_gw", "Link 2 IPV6 gateway").setRdb("link.profile.2.ipv6_gw"),
    staticTextVariable("link_profile_2_ipv6_ipaddr", "Link 2 IPV6 IP address").setRdb("link.profile.2.ipv6_ipaddr"),
    staticTextVariable("link_profile_2_status_ipv4", "Link 2 IPV4 status").setRdb("link.profile.2.status_ipv4"),
    staticTextVariable("link_profile_2_status_ipv6", "Link 2 IPV6 status").setRdb("link.profile.2.status_ipv6"),

    staticTextVariable("link_profile_3_apn", "Link 3 APN").setRdb("link.profile.3.apn"),
    staticTextVariable("link_profile_3_connect_progress", "Link 3 connect progress").setRdb("link.profile.3.connect_progress"),
    staticTextVariable("link_profile_3_dns1", "Link 3 DNS1").setRdb("link.profile.3.dns1"),
    staticTextVariable("link_profile_3_dns2", "Link 3 DNS2").setRdb("link.profile.3.dns2"),
    staticTextVariable("link_profile_3_enable", "Link 3 enabled").setRdb("link.profile.3.enable"),
    staticTextVariable("link_profile_3_gw", "Link 3 gateway").setRdb("link.profile.3.gw"),
    staticTextVariable("link_profile_3_interface", "Link 3 interface").setRdb("link.profile.3.interface"),
    staticTextVariable("link_profile_3_iplocal", "Link 3 local IP").setRdb("link.profile.3.iplocal"),
    staticTextVariable("link_profile_3_ipv6_gw", "Link 3 IPV6 gateway").setRdb("link.profile.3.ipv6_gw"),
    staticTextVariable("link_profile_3_ipv6_ipaddr", "Link 3 IPV6 IP address").setRdb("link.profile.3.ipv6_ipaddr"),
    staticTextVariable("link_profile_3_status_ipv4", "Link 3 IPV4 status").setRdb("link.profile.3.status_ipv4"),
    staticTextVariable("link_profile_3_status_ipv6", "Link 3 IPV6 status").setRdb("link.profile.3.status_ipv6"),

    staticTextVariable("link_profile_4_apn", "Link 4 APN").setRdb("link.profile.4.apn"),
    staticTextVariable("link_profile_4_connect_progress", "Link 4 connect progress").setRdb("link.profile.4.connect_progress"),
    staticTextVariable("link_profile_4_dns1", "Link 4 DNS1").setRdb("link.profile.4.dns1"),
    staticTextVariable("link_profile_4_dns2", "Link 4 DNS2").setRdb("link.profile.4.dns2"),
    staticTextVariable("link_profile_4_enable", "Link 4 enabled").setRdb("link.profile.4.enable"),
    staticTextVariable("link_profile_4_gw", "Link 4 gateway").setRdb("link.profile.4.gw"),
    staticTextVariable("link_profile_4_interface", "Link 4 interface").setRdb("link.profile.4.interface"),
    staticTextVariable("link_profile_4_iplocal", "Link 4 local IP").setRdb("link.profile.4.iplocal"),
    staticTextVariable("link_profile_4_ipv6_gw", "Link 4 IPV6 gateway").setRdb("link.profile.4.ipv6_gw"),
    staticTextVariable("link_profile_4_ipv6_ipaddr", "Link 4 IPV6 IP address").setRdb("link.profile.4.ipv6_ipaddr"),
    staticTextVariable("link_profile_4_status_ipv4", "Link 4 IPV4 status").setRdb("link.profile.4.status_ipv4"),
    staticTextVariable("link_profile_4_status_ipv6", "Link 4 IPV6 status").setRdb("link.profile.4.status_ipv6"),

    staticTextVariable("link_profile_5_apn", "Link 5 APN").setRdb("link.profile.5.apn"),
    staticTextVariable("link_profile_5_connect_progress", "Link 5 connect progress").setRdb("link.profile.5.connect_progress"),
    staticTextVariable("link_profile_5_dns1", "Link 5 DNS1").setRdb("link.profile.5.dns1"),
    staticTextVariable("link_profile_5_dns2", "Link 5 DNS2").setRdb("link.profile.5.dns2"),
    staticTextVariable("link_profile_5_enable", "Link 5 enabled").setRdb("link.profile.5.enable"),
    staticTextVariable("link_profile_5_gw", "Link 5 gateway").setRdb("link.profile.5.gw"),
    staticTextVariable("link_profile_5_interface", "Link 5 interface").setRdb("link.profile.5.interface"),
    staticTextVariable("link_profile_5_iplocal", "Link 5 local IP").setRdb("link.profile.5.iplocal"),
    staticTextVariable("link_profile_5_ipv6_gw", "Link 5 IPV6 gateway").setRdb("link.profile.5.ipv6_gw"),
    staticTextVariable("link_profile_5_ipv6_ipaddr", "Link 5 IPV6 IP address").setRdb("link.profile.5.ipv6_ipaddr"),
    staticTextVariable("link_profile_5_status_ipv4", "Link 5 IPV4 status").setRdb("link.profile.5.status_ipv4"),
    staticTextVariable("link_profile_5_status_ipv6", "Link 5 IPV6 status").setRdb("link.profile.5.status_ipv6"),

    staticTextVariable("link_profile_6_apn", "Link 6 APN").setRdb("link.profile.6.apn"),
    staticTextVariable("link_profile_6_connect_progress", "Link 6 connect progress").setRdb("link.profile.6.connect_progress"),
    staticTextVariable("link_profile_6_dns1", "Link 6 DNS1").setRdb("link.profile.6.dns1"),
    staticTextVariable("link_profile_6_dns2", "Link 6 DNS2").setRdb("link.profile.6.dns2"),
    staticTextVariable("link_profile_6_enable", "Link 6 enabled").setRdb("link.profile.6.enable"),
    staticTextVariable("link_profile_6_gw", "Link 6 gateway").setRdb("link.profile.6.gw"),
    staticTextVariable("link_profile_6_interface", "Link 6 interface").setRdb("link.profile.6.interface"),
    staticTextVariable("link_profile_6_iplocal", "Link 6 local IP").setRdb("link.profile.6.iplocal"),
    staticTextVariable("link_profile_6_ipv6_gw", "Link 6 IPV6 gateway").setRdb("link.profile.6.ipv6_gw"),
    staticTextVariable("link_profile_6_ipv6_ipaddr", "Link 6 IPV6 IP address").setRdb("link.profile.6.ipv6_ipaddr"),
    staticTextVariable("link_profile_6_status_ipv4", "Link 6 IPV4 status").setRdb("link.profile.6.status_ipv4"),
    staticTextVariable("link_profile_6_status_ipv6", "Link 6 IPV6 status").setRdb("link.profile.6.status_ipv6"),

    staticTextVariable("owa_model", "OWA model").setRdb("owa.system.product.model"),
    staticTextVariable("sas_client_enabled_by_ia", "SAS client enabled").setRdb("owa.service.sas_client.enabled_by_ia"),
    staticTextVariable("sas_server_address", "SAS server address").setRdb("sas.server.address"),
    staticTextVariable("sas_cbsdid", "SAS CBSD ID").setRdb("sas.cbsdid"),
    staticTextVariable("sas_config_installCertificationTime", "SAS Install certification time").setRdb("sas.config.installCertificationTime"),
    staticTextVariable("sas_config_url", "SAS Configuration URL").setRdb("sas.config.url"),
    staticTextVariable("sas_grant_0_id", "SAS grant 0 ID").setRdb("sas.grant.0.id"),
    staticTextVariable("sas_grant_0_pci_earfcn", "SAS grant 0 PCI EARFCN").setRdb("sas.grant.0.pci_earfcn"),
    staticTextVariable("sas_registration_request_error_code", "SAS register request error code").setRdb("sas.registration.request_error_code"),
    staticTextVariable("sas_registration_response_code", "SAS registration response code").setRdb("sas.registration.response_code"),
    staticTextVariable("sas_registration_response_data", "SAS registration response data").setRdb("sas.registration.response_data"),
    staticTextVariable("sas_registration_response_message", "SAS registration response message").setRdb("sas.registration.response_message"),
    staticTextVariable("sas_registration_state", "SAS registration state").setRdb("sas.registration.state")
  ].concat(sas_grants)
});

var pageData : PageDef = {
#ifndef V_WEBIF_SPEC_lark
  onDevice : false,
#endif
  title: "DebugParameters",
  authenticatedOnly: true,
  pageObjects: [po],
  menuPos: ["NIT", "DebugParameters"],
  onReady: onDataReady
}

function onDataReady(resp) {
  var sas_client_enabled = resp.debugParameters.sas_client_enabled_by_ia == 1;
  var is_magpie = resp.debugParameters.owa_model.match(/ifwa661$/g) != null;

  $("#div_owa_model").hide();
  if(!sas_client_enabled && !is_magpie) {
    $("[id^=div_sas]").hide();
    return;
  }

  $("#objwrapperdebugParameters").append('<div id="div_toggle_sas_grangts" class="form-row"><label for="inp_sas_toggle_sas_grants">Show all SAS grants</label><div class="field" style="margin:6px 0 0 3px;"><input type="checkbox" id="inp_sas_toggle_sas_grants"></div></div>');

  for(var i=1; i<=15; i++) {
    var obj_id = "#inp_sas_grant_state_" + i;
    var state = $(obj_id).text();
    if(["GRANTED", "AUTHORIZED", "RELINQUISHED"].indexOf(state) == -1) {
      $("#div_sas_grant_state_"+i).hide();
      $("#div_sas_grant_reason_"+i).hide();
      $("#div_sas_grant_next_heartbeat_"+i).hide();
    }
  }

  $("#inp_sas_toggle_sas_grants").change(()=>{
    for(var i=1; i<=15; i++) {
      var obj_id = "#inp_sas_grant_state_" + i;
      var state = $(obj_id).text();
      if(["GRANTED", "AUTHORIZED", "RELINQUISHED"].indexOf(state) == -1) {
        $("#div_sas_grant_state_"+i).toggle();
        $("#div_sas_grant_reason_"+i).toggle();
        $("#div_sas_grant_next_heartbeat_"+i).toggle();
      }
    }
  });
}
