#!/bin/sh

# * output header
#
#
# #define RDB_SYSTEM_BOOT_MODE 0
#	#define RDB_SYSTEM_BOOT_MODE__RECOVERY 0
#	#define RDB_SYSTEM_BOOT_MODE__PRODUCTION 2
#
# extern struct str_to_idx_t rdbvar_to_varidx;
#

# * output source
#
# struct str_to_idx_t rdbvar_to_varidx[]={
# 	{ "system.boot_mode:recovery,production",RDB_SYSTEM_BOOT_MODE },
# 	{ "system.hardware_failure",RDB_SYSTEM_HARDWARE_FAILURE },
# 	{ "wwan.0.radio.information.signal_strength",RDB_RADIO_INFORMATION_SIGNAL_STRENGTH },
# 	{ "wwan.0.system_network_status.service_type",RDB_SYSTEM_NETWORK_STATUS_SERVICE_TYPE },
# 	{ "wwan.0.sim.status.status:SIM OK,SIM PIN,SIM PUK:sim_card_status" },
# 	{ "wwan.0.network_activity",RDB_NETWORK_ACTIVITY },
# 	{ "link.profile.1.status",RDB_LINK_PROFILE_1_STATUS },
# 	{ "link.profile.2.status",RDB_LINK_PROFILE_2_STATUS },
# 	{ "link.profile.3.status",RDB_LINK_PROFILE_3_STATUS },
# 	{ "link.profile.4.status",RDB_LINK_PROFILE_4_STATUS },
# 	{ "link.profile.1.connecting",RDB_LINK_PROFILE_1_CONNECTING },
# 	{ "link.profile.2.connecting",RDB_LINK_PROFILE_2_CONNECTING },
# 	{ "link.profile.3.connecting",RDB_LINK_PROFILE_3_CONNECTING },
# 	{ "link.profile.4.connecting",RDB_LINK_PROFILE_4_CONNECTING },
# 	{ "powersavemode.stat",RDB_POWERSAVEMODE_STAT },
# 	{NULL,-1},
# };
# 
# struct str_to_idx_t rdbval_to_validx[] {
# 	{ "system.boot_mode:recovery", RDB_VAL(RDB_SYSTEM_BOOT_MODE,RECOVERY) },
# 	{ "system.boot_mode:production", RDB_VAL(RDB_SYSTEM_BOOT_MODE,RECOVERY) },
# }
# 
# enum disp_idx_t varidx_to_dispidx[]={
# 	router_power_status, // RDB_SYSTEM_BOOT_MODE (0)
# }
#
#
# * config
# wwan.0.sim.status.status:SIM OK,SIM PIN,SIM PUK:sim_card_status # SIM card status (SIM OK and etc)

cat_conf()  {
	# remove comment, blank line, heading spaces and tailing spaces
	cat rdbnoti.conf | sed 's/^[[:space:]]*\#[^\#]\+//g;s/\#\#/\#/g' | gcc -E -x c -P $gcc_options - -o - | sed 's/#.*$//;s/^[[:space:]]\+//g;s/[[:space:]]\+$//g;/^[[:space:]]*$/d;'
}


sed_to_def() {
	#  replace dots, spaces, dash and slash with underscore and convert to upper cases
	sed 's/\./_/g;s/ /_/g;s/\//_/g;s/\-/_/g;s/\+/P/g;' | tr '[:lower:]' '[:upper:]'
}

sed_to_break() {
	sed "s/$1/\n/g"
}

print_defines() {

	cat << EOF >> "dispd_def.h"
/*
	DO NOT EDIT THIS FILE DIRECTLY. THIS FILE IS GENERATED BY rdbnoti_gen.sh from rdbnoti.conf
*/
	
#ifndef RDBNOTI_DEF_H__
#define RDBNOTI_DEF_H__

//extern const struct str_to_idx_t rdbvar_to_varidx[];
//extern const struct str_to_idx_t rdbval_to_validx[];
//extern const enum disp_idx_t varidx_to_dispidx[];

EOF
	
	i=0
	while read line; do
	
		# awk to var, vals and disp
		eval $(echo "$line" | awk -F ":" "{print \"var='\"\$1\"';\" \"vals='\"\$2\"';\" \"disp='\"\$3\"';\"}")
		# get C define name
		def=$(echo "$var" | sed_to_def)
		
		dispidx="invalid_disp"
		if [ -n "$disp" ]; then
			dispidx="$disp"
		fi
		
		#echo "var=$var vals=$vals disp=$disp def=$def" >&2

		# print define varidx
		echo "// '$var' ==> '$disp'" >> "dispd_def.h"
		echo "#define RDB_$def $i" >> "dispd_def.h"
		# print rdbvar_to_varidx
		echo "	{ \"$var\",RDB_$def }," >> "dispd_def.c.varidx"
		# print varidx_to_dispidx
		echo "	$dispidx, // RDB_${def}($i) - $var" >> "dispd_def.c.varidx_to_dispidx"
		
		j=0
		echo "$vals" | sed_to_break "," | while read val; do
			defval=$(echo "$val" | sed_to_def)
			
			if [ -z "$defval" ]; then
				continue
			fi
			
			# print define validx
			echo "	// $var:$val ==> $j"  >> "dispd_def.h"
			echo "	#define RDB_${def}__${defval} $j" >> "dispd_def.h"
			# print rdbval_to_validx
			echo "	{ \"$var:$val\", RDB_VAL($def,$defval) }," >> "dispd_def.c.validx"
			
			j=$((j+1))
		done
		
		echo >> "dispd_def.h"
		
		
		i=$((i+1))
		
	done << EOF
$(cat_conf)
EOF
	
	echo "	{ NULL, -1 }," >> "dispd_def.c.varidx"
	echo "	{ NULL, -1 }," >> "dispd_def.c.validx"
	
	echo "#define RDB_GEN_COUNT $i" >> "dispd_def.h"
	
	echo "#endif" >> "dispd_def.h"
}
	
	
gen_rdbnoti_defs() {
	# delete previous left-over files
	rm -f "dispd_def.h" "dispd_def.c.validx" "dispd_def.c.varidx" "dispd_def.c.varidx_to_dispidx"
	
	print_defines
	
	cat << EOF > dispd_def.c
	
/*
	DO NOT EDIT THIS FILE DIRECTLY. THIS FILE IS GENERATED BY rdbnoti_gen.sh from rdbnoti.conf
*/
	
#include <string.h>
#include "dispd_def.h"
#include "dispd.h"
	
const struct str_to_idx_t rdbvar_to_varidx[]={
$(cat "dispd_def.c.varidx")
};

const struct str_to_idx_t rdbval_to_validx[]={
$(cat "dispd_def.c.validx")
};
	
const enum disp_idx_t varidx_to_dispidx[]={
$(cat "dispd_def.c.varidx_to_dispidx")
};
EOF
	# delete
	rm -f "dispd_def.c.validx" "dispd_def.c.varidx" "dispd_def.c.varidx_to_dispidx"
}
	
preprocess_get_gcc_options() {
	# collect gcc options
	env | sed -n '
		# replace with underscores
		s/[ \.\/\:\-]/_/g
		# convert to compile options
		s/\(^V_[a-zA-Z0-9_\-]\+\)=['\'']\{0,1\}\([^'\'']*\)['\'']\{0,1\}/-D\1_\2/p
	'
}

# get gcc preprocess options
gcc_options=$(preprocess_get_gcc_options)

gen_rdbnoti_defs

