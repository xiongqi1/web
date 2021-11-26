#!/bin/sh

#
#  Currently we are doing the supplimentary service only in the script but eventually we will do
# all the voice call related customization and we may be moving to # a small tool written in C.
# Plus, we are now doing variant-based localization but in the end, we will get location
# information from the SIM to do automatic localization
#

log() {
	echo "### voicecallenv: $@"
}

platform=`cat /etc/platform.txt | sed "s/platform=//"`

SKIN=`rdb_get system.skin`
BOARD=`rdb_get system.board`

if [ "$platform" = "Platypus" ]; then
		DB_GET="nvram_get"
		DB_SET="nvram_set"
		PROFILE_DB="wwan_0_telephony_profile"
else
		DB_GET="rdb_get"
		DB_SET="rdb_set"
		PROFILE_DB="wwan.0.telephony.profile"
fi

telephony_profile=`$DB_GET $PROFILE_DB`

if [ "$telephony_profile" == "" ]; then
		if [ "$SKIN" == "ts" ] || [ "$SKIN" == "ro" ]; then
				telephony_profile="1"
		elif [ "$SKIN" == "int" ]; then
				telephony_profile="2"
		else
				telephony_profile="0"
		fi
		$DB_SET $PROFILE_DB $telephony_profile
fi

# set this rdb variable for web interface
if [ "$platform" = "Platypus" ]; then
		rdb_set wwan.0.telephony.profile $telephony_profile
fi

# customizing for Australia Telstra
custom_australia() {
	log "Australia (Telstra) supplimentary service applied"

	rdb_set "suppl_rule.call_forward_immediate.activate" "^\\*21\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_immediate.deactivate" "^#21#$"
	rdb_set "suppl_rule.call_forward_immediate.query" "^\\*#21#$"

	rdb_set "suppl_rule.call_forward_busy.activate" "^\\*24\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_busy.deactivate" "^#24#$"
	rdb_set "suppl_rule.call_forward_busy.query" "^\\*#24#$"

	rdb_set "suppl_rule.call_forward_busy2.activate" "^\\*67\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_busy2.deactivate" "^#67#$"
	rdb_set "suppl_rule.call_forward_busy2.query" "^\\*#67#$"

	rdb_set "suppl_rule.call_forward_no_reply.activate" "^\\*61\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply.deactivate" "^#61#$"
	rdb_set "suppl_rule.call_forward_no_reply.query" "^\\*#61#$"

	rdb_set "suppl_rule.call_forward_no_reply2.activate" "^\\*61\\*<number:[0-9]+>\\*\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply2.deactivate" "^#61#$"
	rdb_set "suppl_rule.call_forward_no_reply2.query" "^\\*#61#$"

	rdb_set "suppl_rule.call_forward_not_reachable.activate" "^\\*62\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_not_reachable.deactivate" "^#62#$"
	rdb_set "suppl_rule.call_forward_not_reachable.query" "^\\*#62#$"

	rdb_set "suppl_rule.clip.activate" "^\\*30\\#$"
	rdb_set "suppl_rule.clip.deactivate" "^#30#$"
	rdb_set "suppl_rule.clip.query" "^\\*#30#$"

	# Telstra requirement : per call basis clip, clear here to be processed getFilterDialCommand() in model_default.c
	rdb_set "suppl_rule.clip_per_call.activate" ""
	rdb_set "suppl_rule.clip_per_call.deactivate" ""
	rdb_set "suppl_rule.clip_per_call.query" ""

	rdb_set "suppl_rule.clir.activate" "^\\*31\\#$"
	rdb_set "suppl_rule.clir.deactivate" "^#31#$"
	rdb_set "suppl_rule.clir.query" "^\\*#31#$"

	# Telstra requirement : per call basis clir, clear here to be processed getFilterDialCommand() in model_default.c
	rdb_set "suppl_rule.clir_per_call.activate" ""
	rdb_set "suppl_rule.clir_per_call.deactivate" ""
	rdb_set "suppl_rule.clir_per_call.query" ""

	rdb_set "suppl_rule.call_waiting.activate" "^\\*43\\#$"
	rdb_set "suppl_rule.call_waiting.deactivate" "^#43#$"
	rdb_set "suppl_rule.call_waiting.query" "^\\*#43#$"

	rdb_set "suppl_rule.call_barring_international.activate" "^\\*331\\*<pin:[0-9]+>#$"
	rdb_set "suppl_rule.call_barring_international.deactivate" "^\\#331\\*<pin:[0-9]+>#$"
	rdb_set "suppl_rule.call_barring_international.query" "^\\*#331#$"
}

# customizing for Canadian Telus
custom_canadian() {
	log "Canada supplimentary service applied"

	rdb_set "suppl_rule.call_forward_immediate.activate" "^\\*21\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_immediate.deactivate" "^#21#$"
	rdb_set "suppl_rule.call_forward_immediate.query" "^\\*#21#$"

	rdb_set "suppl_rule.call_forward_busy.activate" "^\\*67\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_busy.deactivate" "^#67#$"
	rdb_set "suppl_rule.call_forward_busy.query" "^\\*#67#$"
		
	if [ "$SKIN" == "ro" ]; then
		rdb_set "suppl_rule.call_forward_busy2.activate" "^\\*24\\*<number:[0-9]+>#$"
		rdb_set "suppl_rule.call_forward_busy2.deactivate" "^#24#$"
		rdb_set "suppl_rule.call_forward_busy2.query" "^\\*#24#$"
	fi

	rdb_set "suppl_rule.call_forward_no_reply.activate" "^\\*61\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply.deactivate" "^#61#$"
	rdb_set "suppl_rule.call_forward_no_reply.query" "^\\*#61#$"

	rdb_set "suppl_rule.call_forward_no_reply2.activate" "^\\*61\\*<number:[0-9]+>\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply2.deactivate" "^#61#$"
	rdb_set "suppl_rule.call_forward_no_reply2.query" ""

	rdb_set "suppl_rule.call_forward_no_reply3.activate" "^\\*61\\*<number:[0-9]+>\\*<bs:[0-9]+>\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply3.deactivate" ""
	rdb_set "suppl_rule.call_forward_no_reply3.query" ""

	rdb_set "suppl_rule.call_forward_no_reply4.activate" "^\\*004\\*<number:[0-9]+>\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply4.deactivate" "^#004#$"
	rdb_set "suppl_rule.call_forward_no_reply4.query" "^\\*#004#$"

	rdb_set "suppl_rule.call_forward_not_reachable.activate" "^\\*62\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_not_reachable.deactivate" "^#62#$"
	rdb_set "suppl_rule.call_forward_not_reachable.query" "^\\*#62#$"

	rdb_set "suppl_rule.call_waiting.activate" "^\\*43\\#$"
	rdb_set "suppl_rule.call_waiting.deactivate" "^#43#$"
	rdb_set "suppl_rule.call_waiting.query" "^\\*#43#$"

	rdb_set "suppl_rule.clip.activate" "^\\*30\\#$"
	rdb_set "suppl_rule.clip.deactivate" "^#30#$"
	rdb_set "suppl_rule.clip.query" "^\\*#30#$"

	rdb_set "suppl_rule.clir.activate" "^\\*31\\#$"
	rdb_set "suppl_rule.clir.deactivate" "^#31#$"
	rdb_set "suppl_rule.clir.query" "^\\*#31#$"

	rdb_set "suppl_rule.clir_per_call.activate" "^\\*31\\#<number:[\\+0-9]+>;$"
	rdb_set "suppl_rule.clir_per_call.deactivate" "^#31#<number:[\\+0-9]+>;$"
	rdb_set "suppl_rule.clir_per_call.query" ""

	rdb_set "suppl_rule.call_barring_international.activate" "^\\*331\\*<pin:[0-9]+>#$"
	rdb_set "suppl_rule.call_barring_international.deactivate" "^\\#331\\*<pin:[0-9]+>#$"
	rdb_set "suppl_rule.call_barring_international.query" "^\\*#331#$"
}

# customizing for North America
profile_north_america() {
	log "North America supplimentary service applied"

	rdb_set "suppl_rule.call_forward_immediate.activate" "^\\*21\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_immediate.deactivate" "^#21#$"
	rdb_set "suppl_rule.call_forward_immediate.query" "^\\*#21#$"

	rdb_set "suppl_rule.call_forward_busy.activate" "^\\*67\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_busy.deactivate" "^#67#$"
	rdb_set "suppl_rule.call_forward_busy.query" "^\\*#67#$"

	rdb_set "suppl_rule.call_forward_no_reply.activate" "^\\*61\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply.deactivate" "^#61#$"
	rdb_set "suppl_rule.call_forward_no_reply.query" "^\\*#61#$"

	rdb_set "suppl_rule.call_forward_no_reply2.activate" "^\\*61\\*<number:[0-9]+>\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply2.deactivate" "^#61#$"
	rdb_set "suppl_rule.call_forward_no_reply2.query" ""

	rdb_set "suppl_rule.call_forward_no_reply3.activate" "^\\*61\\*<number:[0-9]+>\\*<bs:[0-9]+>\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply3.deactivate" ""
	rdb_set "suppl_rule.call_forward_no_reply3.query" ""

	rdb_set "suppl_rule.call_forward_no_reply4.activate" "^\\*004\\*<number:[0-9]+>\\*<timer:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_no_reply4.deactivate" "^#004#$"
	rdb_set "suppl_rule.call_forward_no_reply4.query" "^\\*#004#$"

	rdb_set "suppl_rule.call_forward_not_reachable.activate" "^\\*62\\*<number:[0-9]+>#$"
	rdb_set "suppl_rule.call_forward_not_reachable.deactivate" "^#62#$"
	rdb_set "suppl_rule.call_forward_not_reachable.query" "^\\*#62#$"

	rdb_set "suppl_rule.call_waiting.activate" "^\\*43\\#$"
	rdb_set "suppl_rule.call_waiting.deactivate" "^#43#$"
	rdb_set "suppl_rule.call_waiting.query" "^\\*#43#$"

	rdb_set "suppl_rule.clip.activate" "^\\*30\\#$"
	rdb_set "suppl_rule.clip.deactivate" "^#30#$"
	rdb_set "suppl_rule.clip.query" "^\\*#30#$"

	rdb_set "suppl_rule.clir.activate" "^\\*31\\#$"
	rdb_set "suppl_rule.clir.deactivate" "^#31#$"
	rdb_set "suppl_rule.clir.query" "^\\*#31#$"

	rdb_set "suppl_rule.clir_per_call.activate" "^\\*31\\#<number:[\\+0-9]+>;$"
	rdb_set "suppl_rule.clir_per_call.deactivate" "^#31#<number:[\\+0-9]+>;$"
	rdb_set "suppl_rule.clir_per_call.query" ""

	rdb_set "suppl_rule.call_barring_international.activate" "^\\*331\\*<pin:[0-9]+>#$"
	rdb_set "suppl_rule.call_barring_international.deactivate" "^\\#331\\*<pin:[0-9]+>#$"
	rdb_set "suppl_rule.call_barring_international.query" "^\\*#331#$"
}

log "applying supplimentary rules (profile=$telephony_profile)"

case $telephony_profile in
	'0')
		custom_australia
		;;

	'1')
		custom_canadian
		;;
	
	'2')
		profile_north_america
		;;
	*)
		echo "### voicecallenv.sh: warning - not implemented for skin $telephony_profile"
		;;
esac
