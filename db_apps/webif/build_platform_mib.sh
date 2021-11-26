#!/bin/sh

#
# generate mib file for the platform
#
# currently, this script is using the following V variants
#
# V_SKIN
# V_PRODUCT
# V_BLUETOOTH
#

#
# TODO: ORGANIZATION has to be variable from skin to skin
#

# check V_SKIN
if [ -z "$V_SKIN" -o "$V_SKIN" = "none" ]; then
	echo "V_SKIN not inherited" >&2
	exit 1
fi

# check V_PRODUCT
if [ -z "$V_PRODUCT" -o "$V_PRODUCT" = "none" ]; then
	echo "V_PROUDCT not inherited" >&2
	exit 1
fi


lower() {
	sed "y/ABCDEFGHIJKLMNOPQRSTUVWXYZ_/abcdefghijklmnopqrstuvwxyz-/"
}

upper() {
	 sed "y/abcdefghijklmnopqrstuvwxyz_/ABCDEFGHIJKLMNOPQRSTUVWXYZ-/"
}

removedash() {
	sed "s/-//g"
}


lower_V_SKIN=$(echo "$V_SKIN" | lower )
upper_V_SKIN=$(echo "$V_SKIN" | upper)

lower_V_PRODUCT=$(echo "$V_PRODUCT" | lower | removedash)
upper_V_PRODUCT=$(echo "$V_PRODUCT" | upper | removedash)

cat << EOF
${upper_V_SKIN}-Router-MIB DEFINITIONS ::= BEGIN

IMPORTS
	MODULE-IDENTITY,
	OBJECT-TYPE,
	OBJECT-IDENTITY,
	Unsigned32,
	NOTIFICATION-TYPE,
	enterprises
		FROM SNMPv2-SMI
	NOTIFICATION-GROUP,
	MODULE-COMPLIANCE
		FROM SNMPv2-CONF;

-- *******************************************************************
-- * ${upper_V_SKIN} module
-- *******************************************************************

DisplayString ::= OCTET STRING

${lower_V_SKIN}	MODULE-IDENTITY
	LAST-UPDATED	"201105110000Z"
	ORGANIZATION	"NetComm Wireless Limited"
	CONTACT-INFO "
		 Name: 	NetComm Wireless Limited
		 Address: 18-20 Orion Road, Lane Cove, NSW 2066, Australia
		 State:	NSW
		 Postcode:	2066
		 City:	Sydney
		 Country:	Australia
		 Phone:	+61-2-94242059 
		 e-mail:	info@netcommwireless.com
		 This is added by NTC
		 "
	DESCRIPTION	"The NetComm SNMP MIB Module for the NTC router"
	REVISION	"201105110000Z"
	DESCRIPTION	"Initial version."
	::= { enterprises 24063 }

router			OBJECT IDENTIFIER ::= { ${lower_V_SKIN} 1 }
${lower_V_PRODUCT}	OBJECT IDENTIFIER ::= { router 1 }

ver		OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 1  } 

routerVerHW		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Router Hardware Version"
    ::= { ver 1 }

routerVerFW		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Router Application Version"
    ::= { ver 2 } 

routerSerialNo		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Router Serial Number"
    ::= { ver 3 } 

modem		OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 2  } 

moduleModel		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Modem Model Number"
    ::= { modem 1 } 

moduleVerFW		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Modem Application Version"
    ::= { modem 3 } 

moduleTemperature OBJECT-TYPE
    SYNTAX  	INTEGER 
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Modem current temperature reading in Celsius."
    ::= { modem 4 }
EOF

	if [ "$V_CELL_NW" = "cdma" ]; then
		cat << EOF
modemMDN		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Modem MDN"
    ::= { modem 6 } 

EOF
	fi

	if [ "$V_CELL_NW" = "umts" ]; then
		cat << EOF
modemIMEI OBJECT-TYPE
    SYNTAX  	OCTET STRING (SIZE(16))
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Modem IMEI."
    ::= { modem 5 }


sim		OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 3  } 

simICCID OBJECT-TYPE
    SYNTAX  	OCTET STRING (SIZE(20))
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"SIM ICCID."
    ::= { sim 1 }

simStatus OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "SIM Status as reported by Radio Module"
    ::= { sim 2 } 
EOF
	elif [ "$V_CELL_NW" = "cdma" ]; then
		cat << EOF
modemMEID OBJECT-TYPE
    SYNTAX  	OCTET STRING (SIZE(16))
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Modem MEID."
    ::= { modem 5 }
EOF
	else
		cat << EOF
-- unknown module type for SIM card status - $V_CELL_NW
EOF
	fi


	
	cat << EOF
radio              OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 4 }

radioPSC OBJECT-TYPE
    SYNTAX  	INTEGER (0..65535)
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Radio Primary Scrambling Code."
    ::= { radio 1 }

radioLAC OBJECT-TYPE
    SYNTAX  	INTEGER (0..65535)
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Radio Location Area Code."
    ::= { radio 2 }

radioRAC OBJECT-TYPE
    SYNTAX  	INTEGER (0..65535)
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Radio Routing Area Code."
    ::= { radio 3 }

radioChannelNumber OBJECT-TYPE
    SYNTAX  	INTEGER (0..65535)
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Radio Channel number ((U)ARFCN)."
    ::= { radio 4 }

radioRSSI 	OBJECT-TYPE
    SYNTAX	INTEGER (-128..127)
    MAX-ACCESS  read-only
    STATUS  	current
    DESCRIPTION	"Radio reported RSSI (in dBm)."
    ::= { radio 5 }

radioReportedBand		OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Radio Reported Band"
    ::= { radio 7 } 
EOF
# does not support cellInformation 
	if false; then # if [ "$V_CELL_NW" = "umts" ]; then
		cat << EOF

cellInformation	OBJECT IDENTIFIER ::= { radio 8  } 

	radioNumberOfCells OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS  read-only
		STATUS  	current
		DESCRIPTION	"The number of cells"
		::= { cellInformation 1 }

	primaryScramblingCode	OBJECT IDENTIFIER ::= { cellInformation 2  } 

		primaryScramblingCode1 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Primary Scrambling Code #1"
			::= { primaryScramblingCode 1 }

		primaryScramblingCode2 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Primary Scrambling Code #2"
			::= { primaryScramblingCode 2 }

		primaryScramblingCode3 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Primary Scrambling Code #3"
			::= { primaryScramblingCode 3 }

		primaryScramblingCode4 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Primary Scrambling Code #4"
			::= { primaryScramblingCode 4 }

		primaryScramblingCode5 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Primary Scrambling Code #5"
			::= { primaryScramblingCode 5 }

		primaryScramblingCode6 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Primary Scrambling Code #6"
			::= { primaryScramblingCode 6 }

	rscp	OBJECT IDENTIFIER ::= { cellInformation 3  } 

		rscp1 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Received Signal Code Power (dB) #1"
			::= { rscp 1 }

		rscp2 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Received Signal Code Power (dB) #2"
			::= { rscp 2 }

		rscp3 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Received Signal Code Power (dB) #3"
			::= { rscp 3 }

		rscp4 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Received Signal Code Power (dB) #4"
			::= { rscp 4 }

		rscp5 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Received Signal Code Power (dB) #5"
			::= { rscp 5 }

		rscp6 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Received Signal Code Power (dB) #6"
			::= { rscp 6 }

	ecio	OBJECT IDENTIFIER ::= { cellInformation 4  } 

		ecio1 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Energy per chip per power density (10dB) #1"
			::= { ecio 1 }

		ecio2 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Energy per chip per power density (10dB) #2"
			::= { ecio 2 }

		ecio3 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Energy per chip per power density (10dB) #3"
			::= { ecio 3 }

		ecio4 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Energy per chip per power density (10dB) #4"
			::= { ecio 4 }

		ecio5 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Energy per chip per power density (10dB) #5"
			::= { ecio 5 }

		ecio6 OBJECT-TYPE
			SYNTAX  	INTEGER (0..65535)
			MAX-ACCESS  read-only
			STATUS  	current
			DESCRIPTION	"Energy per chip per power density (10dB) #6"
			::= { ecio 6 }
EOF
	fi

CINTERION_UMTS="0"
if [ "$V_MODULE" = "cinterion" -o  "$V_MODULE" = "PVS8" -o "$V_MODULE" = "BGS2-E" ]; then
	if [ "$V_CELL_NW" = "umts" ]; then
		CINTERION_UMTS="1"
	fi
fi

if [ "$CINTERION_UMTS" = "1" ]; then
	cat << EOF

		radioECN00	OBJECT-TYPE
		SYNTAX  	DisplayString (SIZE(0..80))
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "energy per bit to noise power spectral density ratio"
		::= { radio 10 }

		radioECN01	OBJECT-TYPE
		SYNTAX  	DisplayString (SIZE(0..80))
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "energy per bit to noise power spectral density ratio"
		::= { radio 11 } 

		radioRSCP0	OBJECT-TYPE
		SYNTAX  	DisplayString (SIZE(0..80))
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Received Signal Code Power (dBm)"
		::= { radio 12 } 

		radioRSCP1	OBJECT-TYPE
		SYNTAX  	DisplayString (SIZE(0..80))
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Received Signal Code Power (dBm)"
		::= { radio 13 } 
EOF
else	
	cat << EOF

		radioECIO0	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Energy per chip per power density"
		::= { radio 10 }

		radioECIO1	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Energy per chip per power density"
		::= { radio 11 } 

		radioRSCP0	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Received Signal Code Power (dB)"
		::= { radio 12 } 

		radioRSCP1	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Received Signal Code Power (dB)"
		::= { radio 13 } 

		radioPCI	OBJECT-TYPE
		SYNTAX  	INTEGER (0..503)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "LTE Physical Cell Identity"
		::= { radio 14 } 

		radioRSRP	OBJECT-TYPE
                SYNTAX	INTEGER (-140..-44)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Reference Signal Received Power (in dBm)"
		::= { radio 15 } 

		radioRSRQ	OBJECT-TYPE
                SYNTAX	INTEGER (-19..-3)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Reference Signal Received Quality (dB)"
		::= { radio 16 } 
EOF
fi
	if [ "$V_CELL_NW" = "cdma" ]; then
		cat << EOF

		radioRTTPN	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "PN index for 1x RTT"
		::= { radio 20 } 

		radioEVDOPN	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "PN index for 1x EVDO"
		::= { radio 21 } 

		radioRTTChannel	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Radio channel for CDMA 1x RTT"
		::= { radio 22 } 

		radioEVDOChannel	OBJECT-TYPE
		SYNTAX  	INTEGER (0..65535)
		MAX-ACCESS	read-only
		STATUS	current
		DESCRIPTION "Radio channel for CDMA 1x EVDO"
		::= { radio 23 } 
EOF
	fi
	cat << EOF

link       OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 5 }

linkPLMN OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Link Public Land Mobile Network Name"
    ::= { link 1 } 

linkCoverage OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..20))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Link Coverage"
    ::= { link 2 } 

linkSessionState OBJECT-TYPE
    SYNTAX     INTEGER
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Link Session State"
    ::= { link 3 } 

linkCellId OBJECT-TYPE
    SYNTAX  	INTEGER (0..65535)
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Link Cell Id"
    ::= { link 5 } 

linkPPPConnection OBJECT-TYPE
    SYNTAX     INTEGER (0..1)
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Link PPP Connection Up"
    ::= { link 6 } 

EOF

if [ "$V_GPS" = "y" ]; then
	cat << EOF
gps       OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 7 }

latitude OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Latitude"
    ::= { gps 1 } 

longitude OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Longitude"
    ::= { gps 2 }

EOF
fi

if [ "$V_ODOMETER" = "y" ]; then
	cat << EOF
odometer	OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 8 }

readOdometer OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Odometer reading"
    ::= { odometer 1 }

odometerStartTime OBJECT-TYPE
    SYNTAX     INTEGER (0..4294967295)
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Odometer start time"
    ::= { odometer 2 }

startOdometer OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "start-stop the odometer"
    ::= { odometer 3 }

odometerDispyMiles OBJECT-TYPE
    SYNTAX     INTEGER (0..1)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Odometer display unit on UI"
    ::= { odometer 4 }

resetOdometer OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Reset odometer to zero"
    ::= { odometer 5 }

odometerThreshold OBJECT-TYPE
    SYNTAX     INTEGER (0..100)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Odometer threshold"
    ::= { odometer 6 }
 
EOF
fi

if [ "$V_BLUETOOTH" != "none" ]; then
	cat << EOF
bluetooth       OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 9 }

bluetoothEnable OBJECT-TYPE
    SYNTAX	INTEGER (0..1)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Bluetooth function is  enabled or disabled."
    ::= { bluetooth 1 }

bluetoothDeviceName OBJECT-TYPE
    SYNTAX	OCTET STRING
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Bluetooth device name"
    ::= { bluetooth 2 }

bluetoothPairable OBJECT-TYPE
    SYNTAX	INTEGER (0..1)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Whether bluetooth function is pairable"
    ::= { bluetooth 3 }

bluetoothDiscoverableTimeout OBJECT-TYPE
    SYNTAX	INTEGER
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Bluetooth discoverable timeout. Please refer to the product manual for the list of valid values."
    ::= { bluetooth 4 }

bluetoothPairedDevices OBJECT-TYPE
    SYNTAX	SEQUENCE OF bluetoothPairedDeviceEntry
    MAX-ACCESS	not-accessible
    STATUS	current
    DESCRIPTION	"Table of bluetooth paired devices"
    ::= { bluetooth 5 }

bluetoothPairedDeviceEntry OBJECT-TYPE
    SYNTAX	BluetoothPairedDeviceEntry
    MAX-ACCESS	not-accessible
    STATUS	current
    DESCRIPTION	"A row describing a bluetooth paired device"
    INDEX   { btAddress }
    ::= { bluetoothPairedDevices 1 }

BluetoothPairedDeviceEntry ::= SEQUENCE {
    btAddress	OCTET STRING,
    btName	OCTET STRING
}

btAddress OBJECT-TYPE
    SYNTAX	OCTET STRING (SIZE(0..17))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION	"Address of a paired device"
    ::= { bluetoothPairedDeviceEntry 1 }

btName OBJECT-TYPE
    SYNTAX	OCTET STRING
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION	"Name of a paired device"
    ::= { bluetoothPairedDeviceEntry 2 }

EOF
fi

if [ "$V_SPEED_TEST" = "y" ]; then
	cat << EOF
networkQuality       OBJECT IDENTIFIER ::= { ${lower_V_PRODUCT} 10 }

dateTime OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "last update date & time"
    ::= { networkQuality 1 }

serverInfo OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Server information"
    ::= { networkQuality 2 }

latency OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Network latency"
    ::= { networkQuality 3 }

downSpeed OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Download speed"
    ::= { networkQuality 4 }

upSpeed OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-only
    STATUS	current
    DESCRIPTION "Upload speed"
    ::= { networkQuality 5 }

updateSpeed OBJECT-TYPE
    SYNTAX     INTEGER (0..1)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Update network speed"
    ::= { networkQuality 6 }

EOF
fi

	cat << EOF
common		OBJECT IDENTIFIER ::= { ${lower_V_SKIN} 100 }
snmp_traps       OBJECT IDENTIFIER ::= { common 1 }

sendHeartbeat OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Send heartbeat"
    ::= { snmp_traps 1 }

trapPersistenceTime OBJECT-TYPE
    SYNTAX 	INTEGER (0..65535)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Trap persistence time"
    ::= { snmp_traps 3 }

trapRetransmissionTime OBJECT-TYPE
    SYNTAX 	INTEGER (0..65535)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Trap retransmission time"
    ::= { snmp_traps 4 }

heartbeatInterval OBJECT-TYPE
    SYNTAX 	INTEGER (0..65535)
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Heartbeat interval"
    ::= { snmp_traps 5 }

trapDestination OBJECT-TYPE
    SYNTAX 	DisplayString (SIZE(0..80))
    MAX-ACCESS	read-write
    STATUS	current
    DESCRIPTION "Trap destination IP address"
    ::= { snmp_traps 6 }

EOF

cat << EOF
END
EOF

exit 0
