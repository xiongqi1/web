#!/bin/sh

# ${}

o_fullname=$1

if [ "${o_fullname}x" = "x" ]; then
	exit 1
fi

bail() {
	rm -rf ${workingDir}
	exit 1
}

workingDir='/tmp/tr069VendorCfg/'
password=''

o_filename="`basename ${o_fullname}`"
o_Name=`echo "${o_filename}" | cut -d'.' -f1`
o_Ext=`echo "${o_filename}" | cut -d'.' -f2`

cfgFile="${o_Name}.cfg"
vpnfile="vpn.des3"

rm -rf ${workingDir}
mkdir -p ${workingDir}

dbcfg_export -o ${workingDir}/${cfgFile} -p "${password}" || bail
sed -i '/^tr069\.transfer\.\|^tr069\.event\.\|^tr069\.dhcp\.eth0\./d' "${workingDir}/${cfgFile}" || bail
tar -C /usr/local/cdcs -zcvf - ipsec.d openvpn-keys |openssl des3 -salt -k "${password}" | dd of="${workingDir}/${vpnfile}" || bail
cd ${workingDir} && tar -zcvf tr069VendorCfg.tar.gz ${cfgFile} vpn.des3 || bail

cp ${workingDir}/tr069VendorCfg.tar.gz ${o_fullname} || bail
rm -rf ${workingDir}

exit 0
