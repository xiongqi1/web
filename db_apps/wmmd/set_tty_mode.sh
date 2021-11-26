#!/bin/sh

# Copyright (C) 2019 NetComm Wireless limited.

#
# This script configures TTY mode in Qualcomm mixer to enable or disable Modem's CTM for TTY mode.
#

nof=${0##*/}
source /lib/utils.sh


tty_mode="$1"

print_usage() {
    cat << EOF

set_tty_mode.sh v1.0

This script configures TTY mode in Qualcomm mixer to enable or disable Modem's CTM for TTY mode.

usage>
    set_tty_mode.sh <OFF|HCO|VCO|FULL>

example>
    set_tty_mode.sh OFF
    set_tty_mode.sh FULL

EOF

}

set_voice_mixer_setting() {
    en="$1"

    if [ "$en" = "0" ]; then
        logNotice "disable mixer setting for voice"
    else
        logNotice "enable mixer setting for voice"
    fi

    amix 'PRI_MI2S_RX_Voice Mixer VoiceMMode1' "$en"
    amix 'VoiceMMode1_Tx Mixer PRI_MI2S_TX_MMode1' "$en"

    logNotice "enable Tx DTMF mute"
    amix 'DTMF_Detect Tx mute voice enable' 1 40000 2> /dev/null > /dev/null
}

# check parameters
if [ $# -lt 1 ]; then
    print_usage
    exit 1
fi

# set TTY mode
case "$tty_mode" in
    "OFF"|"HCO"|"VCO"|"FULL")

        # disable mixer setting for voice - voice mixer settings must be reconfigured to apply tty mode immediately.
        set_voice_mixer_setting 0

        logNotice "set tty mode to '$tty_mode'"
        amix 'TTY Mode' "$tty_mode"

        # enable mixer setting for voice
        set_voice_mixer_setting 1
        ;;

    *)
        logErr "incorrect tty mode used (tty_mode='$tty_mode')"
	exit 1
        ;;
esac

exit 0
