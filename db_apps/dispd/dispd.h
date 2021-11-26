#ifndef __DISPD_H__
#define __DISPD_H__

#define RDB_TRIGGER_NAME_BUF	2048
#define DISP_ENT_STR_LEN	64

#define __MERGE(w,x,y,z) w##x##y##z
#define RDB_VAL(var,val) (__MERGE(RDB_,var,__,val))


struct str_to_idx_t {
	const char* str;
	long idx;
};

enum disp_idx_t {
	
	// following disps are directly matching to each corresponding rdb variable in rdbnofi.conf
	signal_strength,
 	service_type,
	service_type2,
  	boot_mode,
	module_name,
	sim_card_status,
	wwan_attach,
	wwan_reg,
    	hwfail_roaming,	
     
     	// cdma status
     	cdma_activated,
     
     	// pppoe status
     	pppoe_en,
	pppoe_stat,
		// wireless mode
		wireless_mode,
	// wlan enable flag
	wlan_enable,
	wireless_sta_radio,
	wireless_ap_radio,
	wireless_traffic,

#if defined(V_WIFI_ralink)
 	/* TODO: make sure this block belongs to V_WIFI_ralink. compile errors fixed based on rdbnoti.conf */
 	wlan_client_enable,
#endif
  
 	// gps
	gps_enable,
	satellite_count,
	satellite_valid,
	//satellite_data,
	// ZigBee
	zigbee_radio,

	factory_reset_mode,

	// follwing disps are made out of multiple rdb variables by their own recipe
 	wwan_online_status,
  	wwan_connecting_status,
	wwan_enable_status,

    // battery
    battery_online,
    battery_status,
    battery_percent,
	
 	// following disps are maintained by dispd itself
	dim_screen,
	booting_delay,
 
 	// following disps are more command-link
 	pattern_dance,
	dispd_disable,
     
	last_disp_ent,
	invalid_disp=(-1),
};

struct disp_t {
	int dirty;
	
    char str[DISP_ENT_STR_LEN]; // String Information
    long no;                    // Number information
};

#endif
