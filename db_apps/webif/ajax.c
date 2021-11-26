/*!
 * Copyright Notice:
 * Copyright (C) 2002-2008 Call Direct Cellular Solutions
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of CDCS
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CDCS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CDCS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/msg.h>
#include <ctype.h>
#include <string.h>
#include "ajax.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/times.h>

#include "string.h"
#include "rdb_util.h"
#include "cdcs_base64.h"

#if (defined PLATFORM_Platypus2) || (defined PLATFORM_Bovine)  || (defined PLATFORM_Serpent)
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#endif

void getVLANs();

/*
#define xtod(c) ((c>='0' && c<='9') ? c-'0' : ((c>='A' && c<='F') ? \
                c-'A'+10 : ((c>='a' && c<='f') ? c-'a'+10 : 0)))
int HextoDec(char *hex)
{
    if (*hex==0) return 0;
    return  HextoDec(hex-1)*16 +  xtod(*hex) ;
}
 int xstrtoi(char *hex)    // hex string to integer
{
    return HextoDec(hex+strlen(hex)-1);
}*/

#define log_INFO(m, ...) syslog(LOG_INFO, m, ## __VA_ARGS__)
#define log_ERR(m, ...) syslog(LOG_ERR, m, ## __VA_ARGS__)
#define log_DEBUG(m, ...) syslog(LOG_DEBUG, m, ## __VA_ARGS__)

char	escapedStr[512*2+1];

// static void escape ( char * i_buf, char * o_buf)
// {
// 	char * src = i_buf, * dest = o_buf;
//
// 	*dest = 0;
//
// 	for (; *src != '\0' ; src++)
// 	{
// 		if ((*src >= 0x30 && *src <= 0x39) || (*src >= 0x40 && *src <= 0x5A) || (*src >= 0x61 && *src <= 0x7A)
// 			|| (*src >= 0x2D && *src <= 0x2F) || (*src == 0x2A) || (*src == 0x2B) || (*src == 0x5F)){
// 			*dest = *src;
// 			dest++;
// 		}
// 		else {
// 			sprintf(dest, "%%%02X", *src);
// 			dest += 3;
// 		}
// 	}
// 	*dest = 0;
// }

static void escape ( char * i_buf, char * o_buf)
{
	char * src = i_buf, * dest = o_buf;

	*dest = 0;

	for (; *src != '\0' ; src++)
	{
		if (*src == '\'' || *src == '"' || *src == '\\'){
			sprintf(dest, "\\%c", *src);
			dest += 2;
		}
		else {

			*dest = *src;
			dest++;
		}
	}
	*dest = 0;
}

#if ((defined PLATFORM_Platypus2) || (defined PLATFORM_Bovine) || (defined PLATFORM_Serpent)) && ( !defined MODE_recovery )
int get_netif_info_raw(int fd,const char* if_name,int cmd,char* buf,int buf_len) {
	struct ifreq ifr;

	// build ioctl struct
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ-1);

	// get ip address
	if(ioctl(fd, cmd, &ifr)<0) {
		//syslog(LOG_ERR,"failed from ioctl (if_name=%s,cmd=0x%08x) - %s",if_name,cmd,strerror(errno));
		return -1;
	}

	if(cmd==SIOCGIFHWADDR) {
		snprintf(buf,buf_len,"%02X:%02X:%02X:%02X:%02X:%02X",
			(unsigned char)ifr.ifr_hwaddr.sa_data[0],
			(unsigned char)ifr.ifr_hwaddr.sa_data[1],
			(unsigned char)ifr.ifr_hwaddr.sa_data[2],
			(unsigned char)ifr.ifr_hwaddr.sa_data[3],
			(unsigned char)ifr.ifr_hwaddr.sa_data[4],
			(unsigned char)ifr.ifr_hwaddr.sa_data[5]);
	}
	else {
		strncpy(buf,inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),buf_len);
		buf[buf_len-1]=0;
	}

	return 0;
}

int get_netif_info(const char* if_name,char* ipaddr,int ipaddr_len,char* netmask,int netmask_len,char* mac,int mac_len) {
	int fd;

	// open sock dgram
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd<0) {
		syslog(LOG_ERR,"failed to open SOCK_DGRAM - %s",strerror(errno));
		return -1;
	}

	// get ip address
	if(ipaddr) {
		if( (get_netif_info_raw(fd, if_name, SIOCGIFADDR,ipaddr,ipaddr_len)) < 0 ) {
			//syslog(LOG_INFO,"failed to get IP address");
			return -1;
		}
	}

	// get netmask
	if(netmask) {
		if( (get_netif_info_raw(fd, if_name, SIOCGIFNETMASK,netmask,netmask_len)) < 0 ) {
			syslog(LOG_INFO,"failed to get netmask");
			return -1;
		}
	}

	// get mac address
	if(mac) {
		if( (get_netif_info_raw(fd, if_name, SIOCGIFHWADDR,mac,mac_len)) < 0 ) {
			syslog(LOG_INFO,"failed to get mac address");
			return -1;
		}
	}

	close(fd);
	return 0;
}
#endif

void lan_str( char* pos, char* mybuf ) {
	if(( *pos != 'u' )||( *(pos+1) != 'r' )||(strcmp(pos, "N/A")==0)) {
		strcpy(mybuf,"Down");
	}
	else {
		strcpy(mybuf,"Up  /  ");
		if( *(pos+2) == 'g' )
			strcat(mybuf,"1000 Mbps   /   ");
		else if( *(pos+2) == 'h' )
			strcat(mybuf,"100.0 Mbps   /   ");
		else
			strcat(mybuf,"10.0 Mbps   /   ");
		if( *(pos+3) == 'f' )
			strcat(mybuf,"FDX");
		else
			strcat(mybuf,"HDX");
	}
}

/*
 * Print the first string into the variable if valid, otherwise print the
 * second string.
 */
void print_or_alt(char *variable, char *string, char *alternative) {
    char *temp = get_single(string);
	if( strcmp( temp, "N/A" ) != 0 ) {
		printf("var %s='%s';", variable, temp);
	}
	else {
		printf("var %s='%s';", variable, get_single(alternative));
	}
}

int is_valid_model_name(const char* model)
{
	if(!model)
		return 0;

	// no signle letter phone module name!
	if(strlen(model)<2)
		return 0;

	if(!strcmp(model,"N/A"))
		return 0;

	// should start with a letter!
	if(!isalpha(model[0]))
		return 0;

	/* from this point, we need a human in the routine! */

	// not model name - many ZTE module has this incorrect product name in USB product configuration
	// ex) ZTEConfiguration, ZTEWCDMATechnologiesMSM, ZTECDMATechnologiesMSM
	if(!strncmp(model,"ZTE",3) && strstr(model,"MSM"))
		return 0;
	if(strstr(model,"Configuration")!=0)
		return 0;

	// Sierra USB-306 has this incorrect product name
	if(!strcasecmp(model,"HSPA Modem"))
		return 0;

	if(!strcasecmp(model,"HUAWEI Mobile"))
		return 0;

	return 1;
}

#if ((defined PLATFORM_Platypus2) || (defined PLATFORM_Bovine)) && ( !defined MODE_recovery )
int openSckIfNeeded() {
	static int main_skfd=-1;
	// bypass if it is already open
	if(!(main_skfd<0))
		return main_skfd;

	if((main_skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR,"getIfIp: open socket error");
		return -1;
	}
	return main_skfd;
}
#endif

char* trim_quotatons(const char* str)
{
	static char result[256];
	int i;

	if(*str=='"')
		str++;

	i=0;
	while((i<sizeof(result)) && *str)
		result[i++]=*str++;
	result[i]=0;

	if( (i>0) && (result[i-1]=='"') )
		result[i-1]=0;

	return result;
}

int is_dod_en(int profile)
{
	char rdb_name[128];

	int dod_en;
	int pf_no;

	// get dod enable status
	snprintf(rdb_name,sizeof(rdb_name),"dialondemand.enable");
	dod_en=atoi(get_single_raw(rdb_name));
	if(!dod_en)
		return 0;

	// if not matching profile
	pf_no=atoi(get_single_raw("dialondemand.profile"));
	if(profile!=pf_no)
		return 0;

	// get profile enable status
	snprintf(rdb_name,sizeof(rdb_name),"link.profile.%d.enable",pf_no);
	return atoi(get_single_raw(rdb_name));
}

// Work out if there are any clients connected to the AP for the selected 'instance'.
// Return 1: some clients, or an error occurred.
// Return 0: no   clients
int anyApClients(int instance) {
#if defined(V_WIFI_backports)
    char str[64];	// 64 is large enough to prevent buffer overflow
    sprintf(str, "iw dev wlan%d station dump 2> /dev/null | grep \"Station\"", instance);
	return (system(str) == 0) ? 1: 0;
#else
	// @todo Add Ralink/etc support if/when required.
	#warning "Note: WebUi status will display WLAN AP box even if no clients connected"
	return 1;
#endif
}

void convertSecTosysLocalTime(char *input, char *output, int outlen, char *format)
{
	time_t rawtime;
	struct tm *ltime;

	if (input == 0 || output == 0 || outlen ==0)
		return;

	output[0] = 0;

	rawtime=atoi(input);
	if (rawtime == 0)
		return;

	ltime=localtime(&rawtime);

	if (format == 0 || strlen(format) == 0)
		strftime(output, outlen,"%d/%m/%Y %T %Z",ltime);
	else
		strftime(output, outlen,format,ltime);
}

int status_refresh(void) {
char *pos;
char *buf_p;
char buf[512];
int i, j;
//int pptpflag = 0;
ulong uptime = 0;

char buf2[64];
char buf3[64];
FILE* fp;

char linebuf[512];
int list;

int usb_enabled;

int openvpn_servermode;
char wan_interface[64];

char failover_interface[32];
char failover_mode[32];
int wwan_profile;
int alwaysConnectWWAN;

char *saveptr1, *saveptr2;
char *token1, *token2;

	strcpy(wan_interface, "N/A"); //Initialize
	fp=fopen("/proc/uptime", "r");
	if( fp ) {
		fgets(buf2, sizeof(buf2), fp);
		uptime = atol(buf2);
		printf("var uptime=%lu;", uptime);
		fclose(fp);
	}
	else {
		printf("var uptime=0;");
	}
#ifndef PLATFORM_Serpent
	printf("var eth0mac='%s';", get_single("systeminfo.mac.eth0") );
#endif
#ifdef PLATFORM_Serpent
	#define SERIALNO_RDB "system.product.sn"
#else
	#define SERIALNO_RDB "uboot.sn"
#endif
	if(atol(get_single(SERIALNO_RDB))==0) {
		printf("var serialnum='';");
	}
	else {
		printf("var serialnum='%s';", get_single(SERIALNO_RDB));
	}
#ifndef PLATFORM_Serpent
	printf("var br0ip='%s';",  get_single("br0.ip"));
	printf("var br0mask='%s';", get_single("br0.mask") );
#endif
#if defined(V_WIFI) || defined(V_WIFI_CLIENT) || defined(V_WIFI_MBSSID)
	printf("var wlan0mac='%s';", get_single("systeminfo.mac.wlan0"));
#endif

#ifdef V_RUNTIME_CONFIG_y
	printf("var rtc_id='%s';", get_single("runtime.config.config_id"));
#endif
	fp=fopen("/etc/version.txt", "r");
	if( fp ) {
		if( fgets(buf, sizeof(buf), fp) )//1st line
			if(*(buf+strlen(buf)-1)=='\n'||*(buf+strlen(buf)-1)=='\r') *(buf+strlen(buf)-1)=0;
		fgets(buf2, sizeof(buf2), fp);
		*buf2=0;
		if( fgets(buf2, sizeof(buf2), fp) )//read 3rd line
			if(*(buf2+strlen(buf2)-1)=='\n'||*(buf2+strlen(buf2)-1)=='\r') *(buf2+strlen(buf2)-1)=0;
		printf("var version='V%s';", buf);
		//printf("var version='%s  %s';", buf, buf2);
		fclose(fp);
	}
	else {
		printf("var version='XXX';");
	}

	// fiddling around with names - too many stupid phone modules that do not report a proper name
	{
		char* cfg_model;
		char* at_model;
		char* udev_model;
		char* udev_cfg;
		char* model_name;
		char* if_type;
		char* proto_type;

		cfg_model = strdup(get_single("webinterface.module_model"));   // hard-coded name from rdb configuration
		at_model = strdup(trim_quotatons(get_single("wwan.0.model"))); // result from AT command (AT+CGMM)
		udev_cfg = strdup(get_single("wwan.0.module_name"));           // vendor-device-id-based hard-coded name
		udev_model = strdup(get_single("wwan.0.product_udev"));        // USB configuraiton - product
		if_type = strdup(get_single("wwan.0.if"));
		proto_type = strdup(get_single("wwan.0.if"));

		model_name=0;

		if(strstr(proto_type,"at")) {
			// take AT model
			if(!is_valid_model_name(model_name) && is_valid_model_name(at_model))
				model_name=at_model;
		}

		// take the cfg model name - we override any module for a certain variant
		if(!is_valid_model_name(model_name) && is_valid_model_name(cfg_model))
			model_name=cfg_model;

		// take udev configurated name
		if(!is_valid_model_name(model_name) && is_valid_model_name(udev_cfg))
			model_name=udev_cfg;

		// take usb configuraiton product
		if(!is_valid_model_name(model_name) && is_valid_model_name(udev_model))
			model_name=udev_model;

		// do not take if it is invalid
		if(!is_valid_model_name(model_name))
			model_name="N/A";

		printf("var moduleModel='%s';", model_name);

		free(if_type);
		free(udev_cfg);
		free(cfg_model);
		free(at_model);
		free(udev_model);
		free(proto_type);
#if ((defined PLATFORM_Platypus2) || (defined PLATFORM_Bovine) || (defined PLATFORM_Serpent)) && ( !defined MODE_recovery ) && ( !defined V_CUSTOM_FEATURE_PACK_Santos )
		char lan_mac[128];
#ifdef V_BRIDGE_none
		const char * ethIface = "eth0";
#else
		const char * ethIface = "br0";
#endif
		if(get_netif_info(ethIface,0,0,0,0,lan_mac,sizeof(lan_mac))>=0) {
			printf("var br0mac='%s';\n", lan_mac);
		}
		else {
			printf("var lan_mac='N/A';\n");
		}
#endif
	}

	printf("var moduleHardwareVersion='%s';", get_single("wwan.0.hardware_version"));
	printf("var moduleFirmwareVersion='%s';", get_single("wwan.0.firmware_version"));
	printf("var moduleFirmwareVerExt='%s';", get_single("wwan.0.firmware_version_cid"));
#ifndef PLATFORM_Serpent
	printf("var pppAddress='%s';", get_single("wwan.0.pppAddress"));
//	printf("pppMask='%s';", get_single("wwan.0.pppMask"));
	printf("var pppPtP='%s';", get_single("wwan.0.pppPtP"));
#endif
//	printf("ipsecStatus='%s';", get_single("wwan.0.ipsecip"));
	printf("var provider='%s';", get_single("wwan.0.system_network_status.network"));
	printf("var hint='%s';", get_single("wwan.0.system_network_status.hint.encoded"));
	printf("var connType='%s';", get_single("wwan.0.system_network_status.service_type"));
	printf("var coverage='%s';", get_single("wwan.0.system_network_status.system_mode"));
//	printf("reason='%s';", get_single("wwan.0.system_network_status.modem_status") );//Connection Status
#ifdef V_CELL_NW_cdma
	printf("var meid='%s';", get_single("wwan.0.meid") );
#else
	printf("var imei='%s';", get_single("wwan.0.imei") );
#endif

#if defined(V_MODULE_MC7354) || defined(V_MODULE_MC7304)
	printf("var module_meid='%s';", get_single("wwan.0.meid") );
	printf("var esn='%s';", get_single("wwan.0.esn") );
	printf("var prlver='%s';", get_single("wwan.0.prlver") );

	printf("var priid_carrier='%s';", get_single("wwan.0.priid_carrier") );
	printf("var priid_config='%s';", get_single("wwan.0.priid_config") );

#endif

	printf("var freq='%s';", get_single("wwan.0.system_network_status.current_band"));
	printf("var csq='%s';", get_single("wwan.0.radio.information.signal_strength"));
	printf("var roamingStatus='%s';", get_single("wwan.0.system_network_status.roaming"));
	printf("var dataRoamingBlocked='%s';", get_single("roaming.data.blocked")); // 1 - connection is blocked due to data roaming. 0 - not blocked
	printf("var networkRegistration='%s';", get_single("wwan.0.system_network_status.reg_stat"));
	printf("var networkRegistrationRejectCause='%s';", get_single("wwan.0.system_network_status.rej_cause"));
	printf("var psattached='%s';", get_single("wwan.0.system_network_status.attached"));

#ifdef V_MANUAL_ROAMING_vdfglobal
	{
		clock_t now;
		struct tms tmsbuf;

		now=times(&tmsbuf)/sysconf(_SC_CLK_TCK);

		/* roaming information - registration status */
		printf("var mrs_msg='%s';", get_single("manualroam.stat.msg"));
		printf("var mrs_nw='%s';", get_single("manualroam.stat.network"));
		printf("var mrs_rssi='%s';", get_single("manualroam.stat.rssi"));
		printf("var mrs_dmin='%s';", get_single("manualroam.stat.delay_min"));
		printf("var mrs_start='%s';", get_single("manualroam.stat.delay_start"));
		printf("var mrs_now='%ld';", now);
		printf("var mrs_cm_sus='%s';", get_single("manualroam.suspend_connection_mgr"));
		printf("var mrs_custom_roam_simcard='%s';", get_single("manualroam.custom_roam_simcard"));
	}

#endif

#ifndef PLATFORM_Serpent
	pos=get_single("manualroam.resetting");
	printf("var manualroamResetting='%s';", pos);
#else
	printf("var manualroamResetting='0';");
#endif
#if defined(V_MEPLOCK)
	pos=get_single("meplock.status"); // network lock shows 'MEP locked' SIN mep lock shows 'PH-NET PIN'
	if(strcmp(pos, "locked")==0)
		strcpy(pos, "MEP locked");
	else
		pos=get_single("wwan.0.sim.status.status");
#else
	pos=get_single("wwan.0.sim.status.status");
#endif
	printf("var simStatus='%s';", pos);

	pos=get_single("wwan.0.sim.status.retries_puk_remaining");
	printf("var pukRetries='%s';", pos);
	printf("var rememberedICCID='%s';", get_single("wwan_pin_ccid"));
	printf("var currentICCID='%s';", get_single("wwan.0.system_network_status.simICCID"));
	printf("var autopin='%s';", get_single("wwan.0.sim.autopin"));

#ifdef PLATFORM_Serpent
	pos=get_single("wwan.0.sim.imsi_lock.status");
	printf("var imsiLockStatus='%s';", pos);
#endif

	#ifdef	V_ROUTER_TERMINATED_PPPOE
	int pppoe_en;
	int pppoe_wanipforward;

	pppoe_en = atoi(get_single("service.pppoe.server.0.enable"));
	pppoe_wanipforward = atoi(get_single("service.pppoe.server.0.wanipforward_enable"));

	if(pppoe_en && pppoe_wanipforward) {
	#else
	pos = get_single("service.pppoe.server.0.enable");
	if( strstr(pos, "1") ) {
	#endif
		pos = get_single("service.pppoe.server.0.status");
		if( strcmp( pos, "N/A" )==0 )
			printf("var pppoeStatus='ENABLED';");
		else
			printf("var ppCannot get control socketpoeStatus='%s';", pos);
	}
	else {
		printf("var pppoeStatus='DISABLED';");
	}

	// get failover information - currently Platypus2 has these rdb variables but eventually Bovine will have
#ifndef PLATFORM_Serpent
	// get interface
	pos=get_single("service.failover.interface");
	strncpy(failover_interface,pos,sizeof(failover_interface));
	failover_interface[sizeof(failover_interface)-1]=0;

	// get mode

	pos=get_single("service.failover.mode");
	strncpy(failover_mode,pos,sizeof(failover_mode));
	failover_mode[sizeof(failover_mode)-1]=0;

	printf("var failover_mode='%s';", failover_mode );
	printf("var failover_interface='%s';", failover_interface );
#endif

#ifdef PLATFORM_Serpent
	usb_enabled=1; // there is no separate usb module on Serpent
#else
	pos = get_single("wwan.0.activated");
	usb_enabled=atoi(pos);
#endif

	#ifdef	V_ROUTER_TERMINATED_PPPOE
	printf("var pppoeAddress='%s';", get_single("service.pppoe.server.0.wanipforward_ip"));
	#else
	printf("var pppoeAddress='%s';", get_single("pppoe.server.0.ipaddress"));
	#endif

	printf("var pppoeapn='%s';", get_single("service.pppoe.server.0.apn"));
	printf("var pppoeService='%s';", get_single_base64("service.pppoe.server.0.service"));
	printf("var current_apn='%s';", get_single("wwan.0.apn.current"));
#ifndef V_MULTIPLE_WWAN_PROFILES_y
	printf("var autoapn='%s';", get_single("webinterface.autoapn"));
#endif
/*	pos=get_single("link.profile.profilenum");
	int default_profile=atoi(pos);
	if(!default_profile) {
		default_profile=1;
	}
	printf("var default_profile='%i';", default_profile);
*/
	printf("var st=[");
	for( i=1,j=0; ; i++ ) {
		sprintf( buf, "link.profile.%d.dev", i );
		pos = get_single( buf );
		if(strncmp( pos, "ipsec.0",7 )==0 )
			continue;
		if(strncmp( pos, "gre.0",5 )==0 )
			continue;
		if( strncmp( pos, "pptp",4 )==0 )
			continue;
		sprintf( buf, "link.profile.%d.status", i );
		pos = get_single( buf );
		strncpy( buf2, pos, sizeof(buf2) );
		sprintf( buf, "link.profile.%d.enable", i );
		pos = get_single( buf );
		if( strncmp(pos, "N/A", 3)==0 )
			break;
		else if( !strchr(pos, '1') )
			continue;

		// get enable
		strncpy(buf3,pos,sizeof(buf3));
		buf3[sizeof(buf3)-1]=0;

		// get device typ
		sprintf( buf, "link.profile.%d.dev", i );
		pos = get_single( buf );
		// get openvpn server mode or wwan profile flags
		openvpn_servermode=openvpn_servermode && !strcmp(pos,"openvpn.0");
		wwan_profile=!strcmp(pos,"wwan.0");

		// do not print any wwan profile if wan mode
		// connection manager does not connect the profile but the status page show is up if the profile exists in the list
		if(wwan_profile && !strcmp(failover_mode,"wan")) {
			continue;
		}

		if(j>0) printf(",");
		j++;

		printf("{");
		printf("'idx':'%i',", i-1);
		printf("'type':'%s',", pos);

		/* print default route */
		sprintf(buf,"link.profile.%d.defaultroute", i);
		pos = get_single(buf);
		printf("'defgw':'%s',", pos);

		// get server mode
		sprintf(buf,"link.profile.%d.vpn_type", i);
		pos = get_single(buf);
		openvpn_servermode=!strcmp(pos,"server");

		pos = get_single("service.failover.alwaysConnectWWAN");
		alwaysConnectWWAN=!strcmp(pos,"enable");

		// assume wwan is down if wwan and usb is not enabled
		if(wwan_profile && !usb_enabled) {
			printf("'enable':%s,", "0");
			printf("'pppStatus':'%s',", "down");
		}
		// waiting status if dod enabled and status is not up
		else if(wwan_profile && is_dod_en(i) && strcmp(buf2,"up")) {
			printf("'enable':%s,", buf3);
			printf("'pppStatus':'%s',", "waiting_dod");
		}
		// assume wwan is waiting when the router failed over to wan
		else if(wwan_profile && !strcmp(failover_interface,"wan") && strcmp(buf2,"up") ) {
			// assume the profile is disabled when wan only mode is selected
			if(!strcmp(failover_mode,"wan")) {
				printf("'enable':%s,", "0");
				printf("'pppStatus':'%s',", "down");
			}
			else {
				printf("'enable':%s,", buf3);
				printf("'pppStatus':'%s',", "waiting");
			}
		}
		else if(wwan_profile && !strcmp(failover_interface,"wlan") && strcmp(buf2,"up") ) {
			printf("'enable':%s,", "0");
			printf("'pppStatus':'%s',", "down");
		}
		else if(wwan_profile && !strcmp(failover_interface,"wwan") && strcmp(buf2,"up") && ! alwaysConnectWWAN && (!strcmp(failover_mode,"wlanlink") || !strcmp(failover_mode,"wlanping"))) {
			printf("'enable':%s,", "0");
			printf("'pppStatus':'%s',", "down");
		}
		// change up status to ready if openvpn server mode
		else if(openvpn_servermode) {
			printf("'enable':%s,", buf3);
			if(!strcmp(buf2,"up"))
				printf("'pppStatus':'%s',", "Ready");
			else if(strlen(buf2) > 0)
				printf("'pppStatus':'%s',", buf2);
			else
				printf("'pppStatus':'down',");
		}
		else {
			printf("'enable':%s,", buf3);
			if(strlen(buf2) > 0)
				printf("'pppStatus':'%s',", buf2);
			else
				printf("'pppStatus':'down',");
		}

		sprintf( buf, "link.profile.%d.name", i );
		pos = get_single( buf );
		escape(pos, escapedStr);
		printf("'name':'%s',", escapedStr);

		/* get pdp failure result */
		sprintf( buf, "link.profile.%d.pdp_result", i );
		pos = get_single( buf );
		escape(pos, escapedStr);
		printf("'pdp_result':'%s',", escapedStr);

#ifndef V_CELL_NW_cdma
		sprintf( buf, "link.profile.%d.apn", i );
		pos = get_single( buf );
		escape(pos, escapedStr);
		printf("'apn':'%s',", escapedStr);

		sprintf( buf, "link.profile.%d.autoapn", i );
		pos = get_single( buf );
		escape(pos, escapedStr);
		printf("'autoapn':'%s',", escapedStr);
#endif
		sprintf( buf, "link.profile.%d.interface", i );
		pos = get_single( buf );
		printf("'interface':'%s',", pos);
		if(strcmp(pos, "N/A") && strncmp(pos, "tun", 3)) {
			strcpy(wan_interface, pos);
		}
		if(*pos) {
			const char* dns;
			int empty = 1;

			printf("\"dnsserver\":\"");
			sprintf(buf, "link.profile.%d.dns1", i);
			dns = get_single_raw(buf);
			if (*dns) {
				empty = 0;
				printf("%s", dns);
			}
			sprintf(buf, "link.profile.%d.dns2", i);
			dns = get_single_raw(buf);
			if (*dns) {
				if (!empty) {
					printf(" ");
				}
				empty = 0;
				printf("%s", dns);
			}
#if !defined(V_IPV6_none) && !defined(V_IPV6_)
			sprintf(buf, "link.profile.%d.ipv6_dns1", i);
			dns = get_single_raw(buf);
			if (*dns) {
				if (!empty) {
					printf(" ");
				}
				empty = 0;
				printf("%s", dns);
			}
			sprintf(buf, "link.profile.%d.ipv6_dns2", i);
			dns = get_single_raw(buf);
			if (*dns) {
				if (!empty) {
					printf(" ");
				}
				empty = 0;
				printf("%s", dns);
			}
#endif
			printf("\",");
		}
		else {
			printf("\"0\",");
			printf("\"dnsserver\":\" \",");
		}
#ifdef V_MULTIPLE_WWAN_PROFILES_y
		sprintf( buf, "link.profile.%u.apn.current", i );
		pos = get_single( buf );
		escape(pos, escapedStr);
		printf("'currentapn':'%s',", escapedStr);
		sprintf( buf, "link.profile.%d.usage_current", i );
		buf_p = get_single( buf );
		pos=strchr( buf_p, '\n');
		if(pos) *pos=0;
		printf( "'current_session':'%s',", buf_p);

		// <start> to display current usage with system local time
		buf2[0]=0;
		buf3[0]=0;
		token1 = strtok(buf_p, ",");
		if (token1) {
			convertSecTosysLocalTime(token1, buf2, sizeof(buf2), "%d/%m/%Y %T %Z");

			token1 = strtok(0, ",");
			if (token1) {
				convertSecTosysLocalTime(token1, buf3, sizeof(buf3), "%d/%m/%Y %T %Z");
			}
		}
		printf( "'current_session_ltime':'%s,%s',", buf2, buf3);
		// < end > to display current usage with system local time

		/*sprintf( buf, "link.profile.%d.usage_total", i );
		buf_p = get_single( buf );
		pos=strchr( buf_p,'\n');
		if(pos) *pos=0;
		printf( "'total_data_usage':'%s',", buf_p);*/
		sprintf( buf, "link.profile.%d.usage_history", i );
		buf_p = get_single( buf );
		pos=strchr( buf_p,'\n');
		if(pos) *pos=0;
		printf("'usage_history':'%s',", buf_p);

		// <start> to display usage history with system local time
		buf2[0]=0;
		buf3[0]=0;
		printf("'usage_history_ltime':'");
		token1 = strtok_r(buf_p, "&", &saveptr1);

		while (token1)
		{
			token2 = strtok_r(token1 , ",", &saveptr2);
			if (token2)
			{
				convertSecTosysLocalTime(token2, buf2, sizeof(buf2), "%d/%m/%Y %T %Z");

				token2 = strtok_r(0, ",", &saveptr2);
				if (token2)
				{
					convertSecTosysLocalTime(token2, buf3, sizeof(buf3), "%d/%m/%Y %T %Z");
				}
			}
			printf( "%s,%s,", buf2, buf3);
			token1 = strtok_r(0, "&", &saveptr1);
			if (token1)
				printf("&");

		}
		printf("',");
		// <end> to display usage history with system local time

#endif
		sprintf( buf, "link.profile.%d.snat", i );
		pos = get_single( buf );
		printf("'snat':'%s',", pos);
		sprintf( buf, "link.profile.%d.iplocal", i );
		pos = get_single( buf );
		printf("'iplocal':'%s',", pos);

		sprintf(buf, "link.profile.%d.ipv6_ipaddr", i);
		pos = get_single( buf );
		printf("'ipv6_ipaddr':'%s',", pos);

		sprintf( buf, "link.profile.%d.ipremote", i );
		pos = get_single( buf );

		if(openvpn_servermode)
			printf("'ipremote':'0.0.0.0'}");
		else
			printf("'ipremote':'%s'}", pos);
	}
	printf("];");

#ifdef PLATFORM_Platypus2
{
	char* wan_dev=NULL;
	char *wan_mode=NULL;
	char *wwan_status=NULL;
	char wan_ip[128];
	char wan_netmask[128];
	char wan_mac[128];

	//Get Active connection status
	memset(wan_ip, 0, 128);

	// get wan if name
	if(!strcmp(get_single_raw("link.profile.0.wan_conntype"),"pppoe")) {
		wan_dev=strdup(get_single_raw("link.profile.0.wan_dev2"));
	}
	else {
		wan_dev=strdup(get_single_raw("link.profile.0.wan_dev"));
	}

	wan_mode=strdup(get_single_raw("link.profile.0.wan_mode"));
	if( !wan_dev[0] || (get_netif_info(wan_dev,wan_ip,sizeof(wan_ip),wan_netmask,sizeof(wan_netmask),wan_mac,sizeof(wan_mac))<0)) {
		memset(wan_ip, 0, 128);
	}
	wwan_status = strdup(get_single_raw("link.profile.wwan.status"));
	if(strcmp(wan_mode, "wan") == 0) {//PHY in WAN mode
		if( strcmp(failover_mode,"wan") == 0 ) {
		//wan only, we only check WAN connection status
			if(strcmp(wan_ip,"")==0)
				printf("var actConn='None';");
			else
				printf("var actConn='WAN';");
		}
		else if( strcmp(failover_mode,"wwan") == 0 ) {
		 //wwan only, we only check WWAN connection
			if(strcmp(wwan_status,"up") == 0)
				printf("var actConn='WWAN';");
			else
				printf("var actConn='None';");
		}
		else if( strcmp(failover_mode,"phylink") == 0 || strcmp(failover_mode,"ping") == 0) {
		//Failover, we need to check WAN first then WWAN
			if(strcmp(wan_ip,"") !=0)
				printf("var actConn='WAN';");
			else if(strcmp(wwan_status,"up") == 0)
				printf("var actConn='WWAN';");
			else
				printf("var actConn='None';");
		}
		else
			printf("var actConn='None';");
	}
	else { //PHY in LAN mode, we only check the WWAN connection

		if(strcmp(wwan_status,"up") == 0)
			printf("var actConn='WWAN';");
		else
			printf("var actConn='None';");
	}
	free(wwan_status);
	free(wan_dev);
	free(wan_mode);
}
#endif

//PPTP
	printf("var pptpSt=[");

	for( i=1,j=0; j<30; i++ ) {
		sprintf( buf, "link.profile.%d.dev", i );
		pos = get_single( buf );
		if(strncmp(pos, "N/A", 3) == 0)
			break;
		else if(strncmp( pos, "pptp", 4 ) != 0 )
			continue;
		else {
			sprintf( buf, "link.profile.%d.enable", i );
			pos = get_single( buf );
			if( *pos == '0' )
				continue;
		}

		if(j>0) printf(",");
		j++;

		sprintf( buf, "link.profile.%d.name", i );
		pos = get_single( buf );
		printf("{'pptpProfile':'%s',", pos);

		sprintf( buf, "link.profile.%d.status", i );
		pos = get_single( buf );
		printf("'pptpStatus':'%s',", pos);

		sprintf( buf, "link.profile.%d.serveraddress", i );
		pos = get_single_base64( buf );
		printf("'serveraddress':'%s',", pos);

		sprintf( buf, "link.profile.%d.iplocal", i );
		pos = get_single( buf );
		printf("'iplocal':'%s',", pos);

		sprintf( buf, "link.profile.%d.ipremote", i );
		pos = get_single( buf );
		printf("'ipremote':'%s'}", pos);
	}
	printf("];");

//ipsec
	printf("var ipsecSt=[");
	// According to /lib/linkProfileLib.sh, max number of supported profiles is 32
	for( i=1,j=0; j<30 && i<=32; i++ ) {
		int wan_if_pf=0;

		sprintf( buf, "link.profile.%d.dev", i );
		pos = get_single( buf );

		if(strncmp(pos, "N/A", 3) == 0)
			continue; // do not break as profile list may not be contiguous
		else if(strncmp( pos, "ipsec.0", 7 ) != 0 )
			continue;
		else {
			sprintf( buf, "link.profile.%d.enable", i );
			pos = get_single( buf );
			if( *pos == '0' )
				continue;
		}

		if(j>0) printf(",");
		j++;

		sprintf( buf, "link.profile.%d.status", i );
		pos = get_single( buf );
		printf("{'ipsecStatus':'%s',", pos);

		sprintf( buf, "link.profile.%d.wan_if", i );
		pos = get_single( buf );
		printf("'ipsecInterface':'%s',", pos);

		sprintf( buf, "link.profile.%d.wan_if_pf", i );
		pos = get_single( buf );
		if (!pos)
			printf("'ipsecWanDev':'%s',", "");
		else {
			wan_if_pf=atoi(pos);

			if (wan_if_pf > 0) {
				sprintf( buf, "link.profile.%d.dev", wan_if_pf);
				pos = get_single( buf );
				printf("'ipsecWanDev':'%s',", pos);
			}
			else
				printf("'ipsecWanDev':'%s',", "");
		}

		sprintf( buf, "link.profile.%d.name", i );
		pos = get_single_base64( buf );
		printf("'ipsecProfile':'%s',", pos);

		sprintf( buf, "link.profile.%d.local_lan", i );
		pos = get_single( buf );
		printf("'local_lan':'%s',", pos);

		sprintf( buf, "link.profile.%d.remote_gateway", i );
		pos = get_single_base64( buf );
		printf("'remote_gateway':'%s',", pos);

		sprintf( buf, "link.profile.%d.remote_lan", i );
		pos = get_single( buf );
		printf("'remote_lan':'%s'}", pos);
	}
	printf("];");
//wlan
#if defined(V_WIFI)
	// Fill in the Wifi AP status.
	printf("var wlanSt=[");

#if defined(V_WIFI_MBSSID)
	#define NUM_APS 5
#else
	#define NUM_APS 1
#endif

	for( i=0; i< NUM_APS; i++ ) {
		if(i>0) printf(",");
		pos=get_single( "wlan.0.radio" );
		printf("{'wlanRadio':'%s'", pos);

		// If the radio is off then skip the rest of the status.
		if (strcmp("1", pos) == 0) {
			sprintf( buf, "wlan.%d.enable", i );
			pos=get_single( buf );
			printf(",'wlanStatus':'%s'", pos);

			pos = (anyApClients(i) != 0) ? "yes": "no";
			printf(",'wlanAnyClients':'%s'", pos);

			sprintf( buf, "wlan.%d.ssid", i );
			pos=get_single( buf );
			escape(pos, escapedStr);
			printf(",'wlanSsid':'%s'", escapedStr);

			sprintf( buf, "wifi.mbssid.%d", i );
			pos=get_single( buf );
			printf(",'wlanMacAddr':'%s'", pos);

			sprintf( buf, "wlan.%d.network_auth", i );
			pos=get_single( buf );
			printf(",'wlanNetworkAuth':'%s'", pos);

			pos=get_single( "wlan.0.conf.channel" );
			if(!strcmp(pos, "0"))
				pos=get_single( "wlan.0.currChan" );
			printf(",'wlanChannel':'%s'", pos);
		}
		printf("}");
	}
	printf("];");

	printf("var wds_enable='%s';\n", get_single("wlan.0.wds_ap_enable"));
	printf("var wds_cli_cnt='%s';\n", get_single("wlan.0.wds_cli_count"));
#endif

#if defined(V_WIFI_CLIENT)
#if defined(V_WIFI_backports) || defined(V_WIFI_qca_soc_lsdk)
	// Note: Backports is the linux driver and the RDBs were changed to support
	// multiple client radios and multiple saved AP profiles.

	// Fill in the Wifi Client status.
	printf("var wifiClientSt=[{");

	pos = get_single( "wlan_sta.0.radio" );
	printf("'wlanRadio':'%s'", pos);

	// If the radio is off then skip the rest of the status.
	if (strcmp("1", pos) == 0) {

		int wds_sta_mode= atoi(get_single_raw( "wlan_sta.0.wds_sta_enable"));

		pos = get_single_base64( "wlan_sta.0.ap.0.ssid" );
		printf(",'wlanRssid':'%s'", pos);

		FILE *outf = popen("wpa_cli status 2>/dev/null |sed -n 's/.*bssid=\\(.*\\)$/\\1/p'","r");
		if (!outf || !fread(buf, 1, 17, outf)) {
			pos = get_single( "wlan_sta.0.ap.0.bssid" );
		}
		else {
			pos=buf;
			*(pos+17)=0;
		}
		pclose(outf);
		printf(",'wlanBssid':'%s'", pos);

		pos = get_single( "wlan_sta.0.ap.0.network_auth" );
		printf(",'wlanSecurity':'%s'", pos);

		if (wds_sta_mode) {
			// There is an RDB variable to tell us if WDS_STA is up or not
			pos = get_single("wlan_sta.0.monitor.wds.connected");
			printf(",'wlanStatus':'%s'", pos);
			printf(",'wlanWdsStatus':'WDS'");
			pos = get_single("wlan_sta.0.monitor.wds.ap_mac");
			// In this mode, we are a bridge, so no WAN IP address, but we can show MAC address
			printf(",'wlanLocalIP':'%s'", pos);
		} else {
			pos = get_single( "wlan_sta.0.sta.connStatus" );
			printf(",'wlanStatus':'%s'", pos);
			printf(",'wlanWdsStatus':'STA'");
			// Get the link profile index and read the local IP address
#if defined(V_WIFI_qca_soc_lsdk)
			strcpy(buf, "wlan_sta.0.ap.0.real_ip");
#else
			pos = get_single( "wlan_sta.0.current_link_profile" );
			strcpy(buf, "link.profile.");
			strcat(buf, pos);
			strcat(buf, ".iplocal");
#endif
			pos = get_single(buf);
			printf(",'wlanLocalIP':'%s'", pos);
#if defined(V_WIFI_qca_soc_lsdk)
			printf(",'wlanDNSServerIP':'%s'", get_single("wlan_sta.0.ap.0.real_dns")); // exists on V_WIFI_qca_soc_lsdk variants only.
#endif
		}
	}
	printf("}];");

#else
    printf("var wifiClientSt=[{");

	pos = get_single( "wlan.0.sta.ssid" );
	printf("'wlanRssid':'%s'", pos);

	pos = get_single( "wlan.0.sta.bssid" );
	printf(",'wlanBssid':'%s'", pos);

	pos = get_single( "wlan.0.sta.network_auth" );
	printf(",'wlanSecurity':'%s'", pos);

	pos = get_single( "wlan.0.sta.connStatus" );
	printf(",'wlanStatus':'%s'", pos);

	pos = get_single( "wlan.0.ip" );
	printf(",'wlanLocalIP':'%s'", pos);

	printf("}];");

#endif	// V_WIFI_backports
#endif	// V_WIFI_CLIENT

#if defined(PLATFORM_AVIAN) || defined(V_MEPLOCK)
	printf("var meplockStatus='%s';", get_single("meplock.status"));
	printf("var meplockResult='%s';", get_single("meplock.result"));
#endif
	printf("var msisdn='%s';", get_single("wwan.0.sim.data.msisdn"));
#ifdef PLATFORM_AVIAN
	printf("var voltage='%s';", get_single("battery.voltage"));
	printf("var capacity='%s';", get_single("battery.percent"));
	printf("var charge='%s';", get_single("battery.status"));

	if( fp=fopen("/sys/devices/platform/qci-battery/power_supply/usb/online", "r") ) {
		fgets(buf, sizeof(buf), fp);
		pos = strchr(buf, '\n');
		if(pos) *pos=0;
		printf("var usbonline='%s';",buf);
		fclose(fp);
	}
	else {
		printf("var usbonline='Unknow';");
	}
#elif defined(V_ADV_STATUS_lv1) || defined(V_ADV_STATUS_lv2)
	printf("var moduleBootVersion='%s';", get_single( "wwan.0.boot_version" ) );
	printf("var PSCs0='%s';", get_single("wwan.0.system_network_status.PSCs0") );	//0x100A
	printf("var LAC='%s';", get_single("wwan.0.system_network_status.LAC") );
	printf("var RAC='%s';", get_single("wwan.0.system_network_status.RAC") );
	printf("var CellId='%s';", get_single("wwan.0.system_network_status.CellID") );
#ifdef PLATFORM_Serpent
	printf("var ChannelNumber='%s';", get_single("wwan.0.system_network_status.channel") );
#else
	printf("var ChannelNumber='%s';", get_single("wwan.0.system_network_status.ChannelNumber") );
#endif
	printf("var simICCID='%s';", get_single("wwan.0.system_network_status.simICCID") );
	printf("var MCC='%s';", get_single( "wwan.0.system_network_status.MCC" ) ); //wwan.0.imsi.plmn_mcc
	printf("var MNC='%02d';", atoi(get_single( "wwan.0.system_network_status.MNC" )) ); //wwan.0.imsi.plmn_mnc
	//printf("var NumberOfRSCP='%d';", reqbuff.NumberOfRSCP);
	//for (n = 0; n<reqbuff.NumberOfRSCP; n++)
	{
		//printf("var PrimScramCodes%s='%d';", n, reqbuff.PrimScramCodes[n] );
		printf("var RSCPs0='%i';", atoi(get_single( "wwan.0.system_network_status.RSCPs0")));//0x700B    reqbuff.RSCPs[n] )
		printf("var ECIOs0='%i';", atoi(get_single( "wwan.0.system_network_status.ECIOs0" )));//n, reqbuff.ECIOs[n] ); ????????
	}
	printf("var IMSI='%s';", get_single( "wwan.0.imsi.msin" ));
	printf("var PRIID_REV='%s';", get_single( "wwan.0.system_network_status.PRIID_REV" ) ); //0x0023 navastr);
	printf("var PRIID_PN='%s';", get_single( "wwan.0.system_network_status.PRIID_PN" ) ); //reqbuff.PRIID+6 );????????????

#ifndef PLATFORM_Serpent
	printf("var RRC='%s';", get_single( "wwan.0.system_network_status.RRC" ) );
	printf("var CGATT='%s';", get_single( "wwan.0.system_network_status.CGATT" ) );
	printf("var RAT='%s';", get_single( "wwan.0.system_network_status.RAT" ) );

	printf("var RXlevel0='%s';", get_single( "wwan.0.radio.information.rx_level0" ) );
	printf("var RXlevel1='%s';", get_single( "wwan.0.radio.information.rx_level1" ) );
#endif
	printf("var rscp0='%s';", get_single( "wwan.0.radio.information.rscp0" ) );
	printf("var rscp1='%s';", get_single( "wwan.0.radio.information.rscp1" ) );
	printf("var ecio0='%s';", get_single( "wwan.0.radio.information.ecio0" ) );
	printf("var ecio1='%s';", get_single( "wwan.0.radio.information.ecio1" ) );
	printf("var hsucat='%s';", get_single( "wwan.0.radio.information.hsucat" ) );
	printf("var hsdcat='%s';", get_single( "wwan.0.radio.information.hsdcat" ) );
#ifdef V_MODCOMMS_y
	printf("var dcvoltage='%s';", get_single( "sys.sensors.info.DC_voltage" ) ); // IoMgr will put the active source here
#else
	printf("var dcvoltage='%s';", get_single( "sys.sensors.io.vin.adc" ) );
#endif
	printf("var dcpoevoltage='%s';", get_single( "sys.sensors.io.3v8poe.adc" ) );
	printf("var powersource='%s';", get_single( "sys.sensors.info.powersource" ) );

	printf("var ECN0_valid='%i';", atoi(get_single( "wwan.0.system_network_status.ECN0_valid" )));
	printf("var ECN0s0='%i';", atoi(get_single( "wwan.0.system_network_status.ECN0s0" )));
	printf("var ECN0s1='%i';", atoi(get_single( "wwan.0.system_network_status.ECN0s1" )));

	printf("var rsrp0='%s';", get_single( "wwan.0.signal.0.rsrp" ) );
	printf("var rsrp1='%s';", get_single( "wwan.0.signal.1.rsrp" ) );
	printf("var rsrq='%s';", get_single( "wwan.0.signal.rsrq" ) );
	printf("var rssinr='%s';", get_single( "wwan.0.signal.rssinr" ) );
	printf("var tac='%s';", get_single( "wwan.0.radio.information.tac" ) );
#ifdef PLATFORM_Serpent
	printf("var pci='%s';", get_single( "wwan.0.system_network_status.PCID" ) );
#else
	printf("var pci='%s';", get_single( "wwan.0.system_network_status.pci" ) );
#endif

#endif

	ulong conn_up_time_point;
	char *connUpTimePointStr=get_single( "wwan.0.conn_up_time_point" ) ;
	if(connUpTimePointStr != NULL) {
		conn_up_time_point= atol(connUpTimePointStr);
		if((uptime > conn_up_time_point) && (conn_up_time_point !=0 ))
			printf("var connuptime=%lu;", uptime-conn_up_time_point);
		else
			printf("var connuptime=0;");
	}
	else {
		printf("var connuptime=0;");
	}

#if defined(V_ADV_STATUS_lv2)
//Fusion Mobile(Sprint Module information)
	printf("var module_manufacturer='%s';",get_single("wwan.0.manufacture"));
	printf("var module_PRL_Version='%s';",get_single("wwan.0.module_info.cdma.prlversion"));
	printf("var module_MDN='%s';",get_single("wwan.0.module_info.cdma.MDN"));
	printf("var module_MSID='%s';",get_single("wwan.0.module_info.cdma.MSID"));
	printf("var module_NAI='%s';",get_single("wwan.0.module_info.cdma.NAI"));
	printf("var module_Available_Data_Network='%s';",get_single("wwan.0.module_info.cdma.ADN"));
	printf("var module_1xRTT_RSSI='%s';",get_single("wwan.0.system_network_status.1xrttrssi"));
	printf("var module_EVDO_RSSI='%s';",get_single("wwan.0.system_network_status.evdorssi"));
	printf("var module_Connection_Status='%s';",get_single("wwan.0.module_info.cdma.connectionstatus"));

//Fusion Mobile(Sprint RF information)
	printf("var module_serviceoption='%s';",get_single("wwan.0.system_network_status.serviceoption"));
	printf("var module_sci='%s';",get_single("wwan.0.system_network_status.slotcycleidx"));
	printf("var module_bandclass='%s';",get_single("wwan.0.system_network_status.bandclass"));
	printf("var module_channelnum='%s';",get_single("wwan.0.system_network_status.1xrttchannel"));
	printf("var module_SID='%s';",get_single("wwan.0.system_network_status.SID"));
	printf("var module_NID='%s';",get_single("wwan.0.system_network_status.NID"));
	printf("var module_1xrtt_aset='%s';",get_single("wwan.0.system_network_status.1xrttaset"));
	printf("var module_evdo_aset='%s';",get_single("wwan.0.system_network_status.evdoaset"));
	printf("var module_1xrtt_cset='%s';",get_single("wwan.0.system_network_status.1xrttcset"));
	printf("var module_evdo_cset='%s';",get_single("wwan.0.system_network_status.evdocset"));
	printf("var module_1xrtt_nset='%s';",get_single("wwan.0.system_network_status.1xrttnset"));
	printf("var module_evdo_nset='%s';",get_single("wwan.0.system_network_status.evdonset"));
	printf("var module_dominantPN='%s';",get_single("wwan.0.system_network_status.dominantPN"));
	printf("var module_evdo_drc='%s';",get_single("wwan.0.system_network_status.evdodrcreq"));
	printf("var module_1xrtt_rssi='%s';",get_single("wwan.0.system_network_status.1xrttrssi"));
	printf("var module_evdo_rssi='%s';",get_single("wwan.0.system_network_status.evdorssi"));
	printf("var module_ecio='%i';",atoi(get_single("wwan.0.system_network_status.ECIOs0")));
	printf("var module_1xrtt_per='%s';",get_single("wwan.0.system_network_status.1xrttper"));
	printf("var module_evdo_per='%s';",get_single("wwan.0.system_network_status.evdoper"));
	printf("var module_txadj='%s';",get_single("wwan.0.system_network_status.txadj"));
#endif

#ifndef PLATFORM_Serpent
	printf("var module_type='%s';", get_single( "wwan.0.module_type" ) );
#endif

	/***** Data usage ****/
#ifndef V_MULTIPLE_WWAN_PROFILES_y
	buf_p = get_single("statistics.wanuptime");
	pos=strchr( buf_p, '\n');
	if(pos) *pos=0;
	printf( "wanuptime='%s';", buf_p);

	buf_p = get_single("statistics.usage_current");
	pos=strchr( buf_p, '\n');
	if(pos) *pos=0;
	printf( "var current_session='%s';", buf_p);

	// <start> to display current usage with system local time
	buf2[0]=0;
	buf3[0]=0;
	token1 = strtok(buf_p, ",");
	if (token1) {
		convertSecTosysLocalTime(token1, buf2, sizeof(buf2), "%d/%m/%Y %T %Z");

		token1 = strtok(0, ",");
		if (token1) {
			convertSecTosysLocalTime(token1, buf3, sizeof(buf3), "%d/%m/%Y %T %Z");
		}
	}
	printf( "var current_session_ltime='%s,%s';\n", buf2, buf3);
	// < end > to display current usage with system local time

	buf_p = get_single("statistics.usage_total");
	pos=strchr( buf_p,'\n');
	if(pos) *pos=0;
	printf( "var total_data_usage='%s';", buf_p);
	buf_p = get_single("statistics.usage_history");
	pos=strchr( buf_p,'\n');
	if(pos) *pos=0;
	printf("var usage_history='%s';\n", buf_p);

	// <start> to display usage history with system local time
	buf2[0]=0;
	buf3[0]=0;
	printf("var usage_history_ltime='");
	token1 = strtok_r(buf_p, "&", &saveptr1);

	while (token1)
	{
		token2 = strtok_r(token1 , ",", &saveptr2);
		if (token2)
		{
			convertSecTosysLocalTime(token2, buf2, sizeof(buf2), "%d/%m/%Y %T %Z");

			token2 = strtok_r(0, ",", &saveptr2);
			if (token2)
			{
				convertSecTosysLocalTime(token2, buf3, sizeof(buf3), "%d/%m/%Y %T %Z");
			}
		}
		printf( "%s,%s", buf2, buf3);
		token1 = strtok_r(0, "&", &saveptr1);
		if (token1)
			printf("&");

	}
	printf("';\n");
	// <end> to display usage history with system local time

#endif
	// openvpn
	printf("var openvpn_clients = [\n");

	fp=fopen("/var/log/openvpn-status.log", "r");
	if(fp) {
		i=list=0;
		while(fgets(linebuf,sizeof(linebuf),fp)) {
#define START_OF_LIST "HEADER,CLIENT_LIST"
#define END_OF_LIST   "HEADER,ROUTING_TABLE"
			if(!strncmp(linebuf,START_OF_LIST,sizeof(START_OF_LIST)-1)) {
				list=1;
				continue;
			}
			else if(!strncmp(linebuf,END_OF_LIST,sizeof(END_OF_LIST)-1)) {
				list=0;
				break;
			}
			if(list) {
				j=0;
				while(linebuf[j]) {
					if( linebuf[j] == '\n' )
						linebuf[j]=0;
					j++;
				}
				if(i==0)
					printf("\"%s\"\n",linebuf);
				else
					printf(",\"%s\"\n",linebuf);
				i++;
			}
		}
		fclose(fp);
	}
	printf("];\n");

// get port status from switchd
printf("var portStatus='");
#if (defined V_ETH_PORT_1pl || defined V_ETH_PORT_1)
	pos = get_single("hw.switch.port.0.status");
	lan_str( pos, buf );
	printf("%s';\n", buf);
#elif (defined V_ETH_PORT_1pl)
	pos = get_single("hw.switch.port.0.status");
	lan_str( pos, buf );
	printf("%s';\n", buf);
#elif (defined V_ETH_PORT_8plllllllw_l) || (defined V_ETH_PORT_2plw_l)
	#if (defined V_ETH_PORT_8plllllllw_l)
	for(i=0; i<8; i++) {
	#else
	for(i=0; i<2; i++) {
	#endif
		if(i) printf(",");
		snprintf(buf2, sizeof(buf2), "hw.switch.port.%d.status", i);
		pos = get_single(buf2);
		lan_str( pos, buf );
		printf("%s", buf);
	}
	printf("';\n");
#elif (defined V_ETH_PORT_4pw_llll) || (defined V_ETH_PORT_4plllw_l) || (defined V_ETH_PORT_2pll)
// get port status from ethmon
{
	char rdb_buf[256];
	char rdb_link[32];
	char rdb_speed[32];
	char rdb_duplex[32];
	int port;

	// Setup our control structures.
	for(port=0; port<6; port++) {
		// get link
		snprintf(rdb_buf,sizeof(rdb_buf),"hw.switch.port.%d.link", port);
		strncpy(rdb_link,get_single_raw(rdb_buf),sizeof(rdb_link));
		rdb_link[sizeof(rdb_link)-1]=0;

		// get speed
		snprintf(rdb_buf,sizeof(rdb_buf),"hw.switch.port.%d.speed", port);
		strncpy(rdb_speed,get_single_raw(rdb_buf),sizeof(rdb_speed));
		rdb_speed[sizeof(rdb_speed)-1]=0;

		// get duplex
		snprintf(rdb_buf,sizeof(rdb_buf),"hw.switch.port.%d.duplex", port);
		strncpy(rdb_duplex,get_single_raw(rdb_buf),sizeof(rdb_duplex));
		rdb_duplex[sizeof(rdb_duplex)-1]=0;

		if(!*rdb_link)
			strcpy(rdb_link,"0");

		if(!*rdb_speed)
			strcpy(rdb_speed,"10");

		if(!*rdb_speed)
			strcpy(rdb_duplex,"H");

		printf("%s,%s,%s,", rdb_link, rdb_speed, rdb_duplex);
	}
	printf("';\n");
}
#elif (defined V_ETH_PORT_0)
	printf("';\n");
#else
	/* default case may be different from V_ETH_PORT_0 */
	/* just to make sure that the enclosing quotation mark is printed */
	printf("';\n");
#endif

#ifdef V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
	getVLANs();
#endif

#ifdef V_CALL_FORWARDING
// voice call information
	printf("var call_waiting='%s';\n", get_single("wwan.0.umts.services.call_waiting.status"));
	printf("var call_forwarding_uncond='%s';\n", get_single("wwan.0.umts.services.call_forwarding.unconditional"));
	printf("var call_forwarding_busy='%s';\n", get_single("wwan.0.umts.services.call_forwarding.busy"));
	printf("var call_forwarding_noreply='%s';\n", get_single("wwan.0.umts.services.call_forwarding.no_reply"));
	printf("var call_forwarding_notreach='%s';\n", get_single("wwan.0.umts.services.call_forwarding.not_reachable"));
#endif

	/* Sierra MC7304 and MC7354 support CDMA and LTE */
	printf("var cdma_sid='%s';\n", get_single("wwan.0.system_network_status.SID"));
	printf("var cdma_nid='%s';\n", get_single("wwan.0.system_network_status.NID"));
	printf("var cdma_mdn='%s';\n", get_single("wwan.0.module_info.cdma.MDN"));
	printf("var module_MSID='%s';",get_single("wwan.0.module_info.cdma.MSID"));
	printf("var cdma_pn='%s';\n", get_single("wwan.0.system_network_status.pn"));
	printf("var cdma_mipinfo='%s';\n", get_single("wwan.0.cdma.perfset.mipinfo"));

#ifdef V_CELL_NW_cdma
	printf("var cdma_activated='%s';\n", get_single("wwan.0.module_info.cdma.activated"));
	printf("var cdma_mip_mode='%s';\n", get_single("wwan.0.cdmamip.mode"));
	printf("var cdma_mip_ip='%s';\n", get_single("wwan.0.cdmamip.ip"));
	printf("var module_1xrttchannel='%s';",get_single("wwan.0.system_network_status.1xrttchannel"));
	printf("var module_1xevdochannel='%s';",get_single("wwan.0.system_network_status.1xevdochannel"));
	printf("var cdma_rttpn='%s';\n", get_single("wwan.0.system_network_status.1xrttpn"));
	printf("var cdma_evdopn='%s';\n", get_single("wwan.0.system_network_status.1xevdopn"));
#elif defined V_WEBIF_VERSION_v2
	pos=get_single("wwan.0.currentband.current_selband");
	if(strcmp( pos, "N/A" )==0) {
		system("rdb_set wwan.0.currentband.cmd.command get");
	}
	/* MC7304 and MC7354 can have incorrect band mask after switching PRI */
	if(strstr(pos,"Use AT!BAND to set band")) {
		pos="N/A";
	}
	printf("var current_selband='%s';\n", pos);
	printf("var plmn_selection_mode='%s';\n", get_single("wwan.0.PLMN_selectionMode"));
#endif

#if defined(V_EVENT_NOTIFICATION)
	printf("var noti_cnt='%s';\n", get_single("service.eventnoti.conf.noti_cnt"));
#endif

#ifdef V_CELL_INFO_WEBUI_y
	{
		int qty, i;
		char rdbk[64];
		qty = atoi(get_single_raw("wwan.0.cell_measurement.qty"));
		printf("var cell_meas_qty=%d;\n", qty);
		printf("var cell_meas=[");
		for (i = 0; i < qty; i++) {
			if (i > 0) {
				printf(",");
			}
			snprintf(rdbk, sizeof(rdbk), "wwan.0.cell_measurement.%d", i);
			printf("'%s'", get_single_raw(rdbk));
		}
		printf("];");
	}
#endif

	return (0);
}
#if (defined PLATFORM_Platypus2) || (defined PLATFORM_Bovine)
#include	<linux/wireless.h>
#include	<arpa/inet.h>

//#define SIOCDEVPRIVATE				0x8BE0
//#define SIOCIWFIRSTPRIV				SIOCDEVPRIVATE
#define RTPRIV_IOCTL_GET_MAC_TABLE		(SIOCIWFIRSTPRIV + 0x0F)

typedef union _MACHTTRANSMIT_SETTING {
	struct  {
		unsigned short  MCS:7;  // MCS
		unsigned short  BW:1;   //channel bandwidth 20MHz or 40 MHz
		unsigned short  ShortGI:1;
		unsigned short  STBC:2; //SPACE
		unsigned short  rsv:3;
		unsigned short  MODE:2; // Use definition MODE_xxx.
	} field;
	unsigned short      word;
} MACHTTRANSMIT_SETTING;

#if (defined PLATFORM_Bovine)
#define MAC_ADDR_LENGTH 6
typedef struct _RT_802_11_MAC_ENTRY {
	unsigned char ApIdx;
	unsigned char Addr[MAC_ADDR_LENGTH];
	unsigned char Aid;
	unsigned char Psm;		/* 0:PWR_ACTIVE, 1:PWR_SAVE */
	unsigned char MimoPs;		/* 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled */
	char AvgRssi0;
	char AvgRssi1;
	char AvgRssi2;
	unsigned int ConnectedTime;
	MACHTTRANSMIT_SETTING TxRate;
} RT_802_11_MAC_ENTRY, *PRT_802_11_MAC_ENTRY;
#else
typedef struct _RT_802_11_MAC_ENTRY {
	unsigned char ApIdx;
	unsigned char Addr[6];
	unsigned char Aid;
	unsigned char Psm;     // 0:PWR_ACTIVE, 1:PWR_SAVE
	unsigned char MimoPs;  // 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
	char		AvgRssi0;
	char		AvgRssi1;
	char		AvgRssi2;
	unsigned int		ConnectedTime;
    MACHTTRANSMIT_SETTING	TxRate;
/* CAUTION : wireless driver source is different between platypus2 and bovine.
 * Those three variables below exist in Platypus2 only.
 */
#if (defined PLATFORM_Platypus2)
//#ifdef RTMP_RBUS_SUPPORT
	unsigned int LastRxRate;
	int		StreamSnr[3];
	int		SoundingRespSnr[3];
//#endif // RTMP_RBUS_SUPPORT //
#endif
} RT_802_11_MAC_ENTRY;
#endif

typedef struct _RT_802_11_MAC_TABLE {
	unsigned long            Num;
	RT_802_11_MAC_ENTRY      Entry[32]; //MAX_LEN_OF_MAC_TABLE = 32
} RT_802_11_MAC_TABLE, *PRT_802_11_MAC_TABLE;

static void convertHwAddrToStr(char* pDst,const char hwAddr[]) {
	sprintf(pDst,"%02x:%02x:%02x:%02x:%02x:%02x",(unsigned char)hwAddr[0],(unsigned char)hwAddr[1],(unsigned char)hwAddr[2],(unsigned char)hwAddr[3],(unsigned char)hwAddr[4],(unsigned char)hwAddr[5]);
}

int is_exist_interface_name(char *if_name) {
	int fd;
	char device[255];
	sprintf(device, "/sys/class/net/%s", if_name);
	fd = open(device, O_RDONLY, 0);
	if (fd < 0)
		return 0;
	else
		close(fd);
	return 1;
}

int print_wan_info() {
#if ( !defined MODE_recovery )
	char* wan_dev=0;
	char* wan_dev_eth=0;
	char* wan_dns=0;
	char* wan_gateway=0;

	char* wan_dns1;
	char* wan_dns2;
	char* wan_gateway1;

	char wan_ip[128];
	char wan_netmask[128];
	char wan_mac[128];

	char rdb_var[128];

	char* conn_type;
	char* wan_type;
	int wan_mode;

	// print wan_mode - connection type
	conn_type=get_single("link.profile.0.wan_mode");
	if(!conn_type || !*conn_type) {
		syslog(LOG_ERR,"wan_mode not exist - link.profile.0.wan_mode");
		goto err;
	}

	//syslog(LOG_INFO,"link.profile.0.wan_mode=%s",conn_type);
	wan_mode=!strcmp(conn_type,"wan");

	// get wan connection type
	wan_type=get_single("link.profile.0.wan_conntype");
	if(!wan_type || !*wan_type) {
		syslog(LOG_ERR,"wan connect type not exist - link.profile.0.wan_conntype");
		goto err;
	}

	printf("var connTypeWan='%s';", wan_type);

	// get wan if name
	if(!strcmp(get_single_raw("link.profile.0.wan_conntype"),"pppoe")) {
		wan_dev=strdup(get_single_raw("link.profile.0.wan_dev2"));
	}
	else {
		wan_dev=strdup(get_single_raw("link.profile.0.wan_dev"));
	}
	wan_dev_eth=strdup(get_single_raw("link.profile.0.wan_dev"));

	// skip if no wan dev found
	if(!wan_dev || !*wan_dev || !wan_dev_eth || !*wan_dev_eth) {
		//syslog(LOG_INFO,"skipping wan information - link.profile.0.wan_dev not exist");
		goto err;
	}

	//syslog(LOG_INFO,"link.profile.0.wan_dev=%s",wan_dev);

	// get interface information
	if(!wan_mode) {
		//syslog(LOG_INFO,"skipping wan infomration - not wan mode");
		goto fini;
	}

	if(get_netif_info(wan_dev,wan_ip,sizeof(wan_ip),wan_netmask,sizeof(wan_netmask),0,0)<0) {
		//syslog(LOG_ERR,"failed to read wan wan info (%s)",wan_dev);
		goto err;
	}

	if(get_netif_info(wan_dev_eth,0,0,0,0,wan_mac,sizeof(wan_mac))<0) {
		//syslog(LOG_ERR,"failed to read wan wan info (%s)",wan_dev);
		goto err;
	}

	// print wan ip, submask and mac address
	printf("var WanIp='%s';", wan_ip);
	printf("var subMask='%s';", wan_netmask);
	printf("var MacAdd='%s';", wan_mac);

	// get dns
	snprintf(rdb_var,sizeof(rdb_var),"nwinf.%s.dns",wan_dev);
	wan_dns=strdup(get_single_raw(rdb_var));

	//syslog(LOG_INFO,"%s='%s'",rdb_var,wan_dns);

	wan_dns1=strtok(wan_dns," ");
	wan_dns2=strtok(0," ");

	printf("var DNS1='%s';", wan_dns1?wan_dns1:"" );
	printf("var DNS2='%s';", wan_dns2?wan_dns2:"" );

	// get gateway
	snprintf(rdb_var,sizeof(rdb_var),"nwinf.%s.router",wan_dev);
	wan_gateway=strdup(get_single_raw(rdb_var));

	//syslog(LOG_INFO,"%s='%s'",rdb_var,wan_gateway);

	wan_gateway1=strtok(wan_gateway," ");
	printf("var Gateway='%s';", wan_gateway?wan_gateway:"");

fini:
	if(wan_dns)
		free(wan_dns);
	if(wan_gateway)
		free(wan_gateway);
	if(wan_dev)
		free(wan_dev);
	if(wan_dev_eth)
		free(wan_dev_eth);
	return 0;

err:
	if(wan_dns)
		free(wan_dns);
	if(wan_gateway)
		free(wan_gateway);
	if(wan_dev)
		free(wan_dev);
	return -1;
#else
	return 0;
#endif
}

//-------------------WPS function ---------------------//
#if ( !defined MODE_recovery )

#define RTPRIV_IOCTL_WSC_PROFILE	(SIOCIWFIRSTPRIV + 0x12)
#define RT_OID_WSC_QUERY_STATUS		0x0751
#define RT_PRIV_IOCTL				(SIOCIWFIRSTPRIV + 0x01)
#define PACKED  __attribute__ ((packed))
#define USHORT  unsigned short
#define UCHAR   unsigned char
#define RT_OID_WSC_PIN_CODE 0x0752

typedef struct PACKED _WSC_CONFIGURED_VALUE {
    USHORT WscConfigured; // 1 un-configured; 2 configured
    UCHAR   WscSsid[32 + 1];
    USHORT WscAuthMode; // mandatory, 0x01: open, 0x02: wpa-psk, 0x04: shared, 0x08:wpa, 0x10: wpa2, 0x
    USHORT  WscEncrypType;  // 0x01: none, 0x02: wep, 0x04: tkip, 0x08: aes
    UCHAR   DefaultKeyIdx;
    UCHAR   WscWPAKey[64 + 1];
} WSC_CONFIGURED_VALUE;

unsigned int getAPPIN(char *interface) {
int socket_id;
struct iwreq wrq;
unsigned int data = 0;
	socket_id = openSckIfNeeded();
	strcpy(wrq.ifr_name, interface);
	if ( !is_exist_interface_name((char *)wrq.ifr_name) )
		return data;
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) &data;
	wrq.u.data.flags = RT_OID_WSC_PIN_CODE;

	if( ioctl(socket_id, RT_PRIV_IOCTL, &wrq) == -1)
		syslog(LOG_ERR,"ioctl error");
	return data;
}

int getWscProfile(char *interface, WSC_CONFIGURED_VALUE *data, int len) {
	struct iwreq wrq;

	int socket_id = openSckIfNeeded();
	strcpy((char *)data, "get_wsc_profile");
	strcpy(wrq.ifr_name, interface);
	if ( !is_exist_interface_name((char *)wrq.ifr_name) )
		return 0;

	wrq.u.data.length = len;
	wrq.u.data.pointer = (caddr_t) data;
	wrq.u.data.flags = 0;
	if(ioctl(socket_id, RTPRIV_IOCTL_WSC_PROFILE, &wrq)<0) {
		//syslog(LOG_ERR,"ioctl error - %s",strerror(errno));
		return -1;
	}

	return 0;
}

void getWPSAuthMode(WSC_CONFIGURED_VALUE *result, char *ret_str) {
	if(result->WscAuthMode & 0x1)
		strcat(ret_str, "Open");
	if(result->WscAuthMode & 0x2)
		strcat(ret_str, "WPA-PSK");
	if(result->WscAuthMode & 0x4)
		strcat(ret_str, "Shared");
	if(result->WscAuthMode & 0x8)
		strcat(ret_str, "WPA");
	if(result->WscAuthMode & 0x10)
		strcat(ret_str, "WPA2");
	if(result->WscAuthMode & 0x20)
		strcat(ret_str, "WPA2-PSK");
}

/*
 * these definitions are from rt2860v2 driver include/wsc.h
 */
char *getWscStatusStr(int status) {
	char* iDle="Idle";

	switch(status){
	case 0:
		return "Not used";
	case 1:
		return iDle;
	case 2:
		return "WSC Fail(Ignore this if Intel/Marvell registrar used)";
	case 3:
		return "Start WSC Process";
	case 4:
		return "Received EAPOL-Start";
	case 5:
		return "Sending EAP-Req(ID)";
	case 6:
		return "Receive EAP-Rsp(ID)";
	case 7:
		return "Receive EAP-Req with wrong WSC SMI Vendor Id";
	case 8:
		return "Receive EAPReq with wrong WSC Vendor Type";
	case 9:
		return "Sending EAP-Req(WSC_START)";
	case 10:
		return "Send M1";
	case 11:
		return "Received M1";
	case 12:
		return "Send M2";
	case 13:
		return "Received M2";
	case 14:
		return "Received M2D";
	case 15:
		return "Send M3";
	case 16:
		return "Received M3";
	case 17:
		return "Send M4";
	case 18:
		return "Received M4";
	case 19:
		return "Send M5";
	case 20:
		return "Received M5";
	case 21:
		return "Send M6";
	case 22:
		return "Received M6";
	case 23:
		return "Send M7";
	case 24:
		return "Received M7";
	case 25:
		return "Send M8";
	case 26:
		return "Received M8";
	case 27:
		return "Processing EAP Response (ACK)";
	case 28:
		return "Processing EAP Request (Done)";
	case 29:
		return "Processing EAP Response (Done)";
	case 30:
		return "Sending EAP-Fail";
	case 31:
		return "WSC_ERROR_HASH_FAIL";
	case 32:
		return "WSC_ERROR_HMAC_FAIL";
	case 33:
		return "WSC_ERROR_DEV_PWD_AUTH_FAIL";
	case 34:
		return "Configured";
	case 35:
		return "SCAN AP";
	case 36:
		return "EAPOL START SENT";
	case 37:
		return "WSC_EAP_RSP_DONE_SENT";
	case 38:
		return "WAIT PINCODE";
	case 39:
		return "WSC_START_ASSOC";
	case 0x101:
		return "PBC:TOO MANY AP";
	case 0x102:
		return "PBC:NO AP";
	case 0x103:
		return "EAP_FAIL_RECEIVED";
	case 0x104:
		return "EAP_NONCE_MISMATCH";
	case 0x105:
		return "EAP_INVALID_DATA";
	case 0x106:
		return "PASSWORD_MISMATCH";
	case 0x107:
		return "EAP_REQ_WRONG_SMI";
	case 0x108:
		return "EAP_REQ_WRONG_VENDOR_TYPE";
	case 0x109:
		return "PBC_SESSION_OVERLAP";
	default:
		return "Unknown";
	}
}

int getWscStatus(char *interface) {
	struct iwreq wrq;
	int data = 0;
	int socket_id = openSckIfNeeded();
	strcpy(wrq.ifr_name, interface);
	if ( !is_exist_interface_name((char *)wrq.ifr_name) )
		return data;
	wrq.u.data.length = sizeof(data);
	wrq.u.data.pointer = (caddr_t) &data;
	wrq.u.data.flags = RT_OID_WSC_QUERY_STATUS;
	if( ioctl(socket_id, RT_PRIV_IOCTL, &wrq) == -1)
		syslog(LOG_ERR,"ioctl error %s", interface);
	return data;
}

void getWPSEncrypType(WSC_CONFIGURED_VALUE *result, char *ret_str) {
	if(result->WscEncrypType & 0x1)
		strcat(ret_str, "None");
	if(result->WscEncrypType & 0x2)
		strcat(ret_str, "WEP");
	if(result->WscEncrypType & 0x4)
		strcat(ret_str, "TKIP");
	if(result->WscEncrypType & 0x8)
		strcat(ret_str, "AES");
}

void updateWPS() {
	int i;
	char tmp_str[128];

	WSC_CONFIGURED_VALUE result;

	if(getWscProfile("ra0", &result, sizeof(WSC_CONFIGURED_VALUE))<0) {
		return;
	}

	//1. WPSConfigured
	printf("var WscConfigured=%d;\n", result.WscConfigured);

	//2. WPSSSID
	if(strchr((char*)result.WscSsid, '\n'))
		printf("var WscSsid='Invalid SSID';");
	else
		printf("var WscSsid='%s';\n", result.WscSsid);

	//3. WPSAuthMode
	tmp_str[0] = '\0';
	getWPSAuthMode(&result, tmp_str);
	printf("var AuthMode='%s';\n", tmp_str);

	//4. EncrypType
	tmp_str[0] = '\0';
	getWPSEncrypType(&result, tmp_str);
	printf("var EncrypType='%s';\n", tmp_str);

	//5. DefaultKeyIdx
	printf("var DefaultKeyIdx='%d';\n", result.DefaultKeyIdx);

	//6. Key
	printf("var WscWPAKey='");
    for(i=0; i<64; i++) {	// WPA key default length is 64 (defined & hardcode in driver)
		if(!isprint(result.WscWPAKey[i]))
			break;
    	//if(i!=0 && !(i % 32))
    	//	printf("\n");
    	printf("%c", result.WscWPAKey[i]);
	}
	printf("';\n");
	//7. WSC Status
	printf("var WscStatus='%s';\n", getWscStatusStr(getWscStatus("ra0")));
	//8. APPIN
	printf("var apPIN='%d';\n", getAPPIN("ra0") );
	//9. WSC Result
	//printf("var WscResult='%d';"), g_WscResult);
	return;
}

int wlInfo(void) {
#if defined(V_WIFI_backports) || defined(V_WIFI_qca_soc_lsdk)
	char buffer[512];
	size_t nr=1;
	FILE *outf = popen("/usr/bin/sta_info.lua","r");
	if (!outf) return -1;
	while (nr) {
		nr = fread(buffer,1,512,outf);
		if (nr) fwrite(buffer, 1, nr, stdout);
	}
	pclose(outf);
	//system("/usr/bin/sta_info.lua");
	return 0;
}
#else /* V_WIFI_backports */
	ulong uptime;
	unsigned int upday;
	unsigned int uphr;
	unsigned int upmin;
	unsigned int upsec;
	char uptimeStr[64];
	int i;
	struct iwreq iwr;
	RT_802_11_MAC_TABLE table = {0};
	char buf[128];
	int s = openSckIfNeeded();
	FILE* fp;
	unsigned long expires;
	char achHwAddr[6*2+5+1];
	unsigned char ip[20];
	unsigned char host[64];
	char* p_Token;
#define MAX_SSID_NUM	5
	unsigned char active_ssid_list[MAX_SSID_NUM] = {0,};
	int ssid_idx = 0;

	//strncpy(iwr.ifr_name, "ra0", IFNAMSIZ);
	strncpy(iwr.ifr_name, "ra0", IFNAMSIZ);
	//syslog(LOG_ERR,"iwr.ifr_name = %s", iwr.ifr_name);
	iwr.u.data.pointer = (caddr_t) &table;
	if ( !is_exist_interface_name((char *)iwr.ifr_name) ) {
		syslog(LOG_ERR,"interface not exist!");
		return -1;
	}
	if (s < 0) {
		syslog(LOG_ERR,"ioctl sock failed!");
		return -1;
	}

	if (ioctl(s, RTPRIV_IOCTL_GET_MAC_TABLE, &iwr) < 0) {
		syslog(LOG_ERR,"ioctl -> RTPRIV_IOCTL_GET_MAC_TABLE failed!");
		close(s);
		return -1;
	}

	/* find enabled SSID list to match with real SSID
	 * Ralink driver has a problem with multi-SSID as below;
	 * 	1) If has 5 SSID and enabled all, there is no problem. Ra 0~5 interfaces are shown and
	 *     these interface indices match with table.Entry[i].ApIdx in order.
	 * 	2) If has 5 SSID and enabled some of them, there is mismatching problem. For example,
	 * 	   enabled SSID 1 and SSID 3, then Ra0 and Ra1 interfaces are shown but table.Entry[i].ApIdx
	 * 	   directs index of Rax interface rather than index of SSID.
	 * Therefore scanning currently enabled SSID list and matching with Rax interface are needed here
	 * to display right SSID on WEB page.
	 */
	for (i=0; i<MAX_SSID_NUM; i++) {
		sprintf( buf,"wlan.%d.enable", i);
		if (atoi(get_single(buf)) == 1) {
			active_ssid_list[ssid_idx++] = i;
			//log_ERR("enabled SSID index = %d", i);
		}
	}

	printf("var wlst=[");
	//log_ERR("table.Num = %ld", table.Num);
	for (i = 0; i < table.Num; i++) {
		if( i>0 )
			printf(",");

		/* Now all Rax interface are on always regardless of its enable flag,
		 * so we can use table.Entry[i].ApIdx as its SSID index again.
		 */
		sprintf( buf,"wlan.%d.ssid", table.Entry[i].ApIdx);
		/* As explained above, use SSID matching real Rax interface */
		//sprintf( buf,"wlan.%d.ssid", active_ssid_list[table.Entry[i].ApIdx]);

		//log_ERR("----------- table index = %d --------------", i);
		//log_ERR("table.Entry[%d].ApIdx         : %d", i, table.Entry[i].ApIdx);
		//log_ERR("table.Entry[%d].Addr          : %s", i, table.Entry[i].Addr);
		//log_ERR("table.Entry[%d].Aid           : %d", i, table.Entry[i].Aid);
		//log_ERR("table.Entry[%d].Psm           : %d", i, table.Entry[i].Psm);
		//log_ERR("table.Entry[%d].MimoPs        : %d", i, table.Entry[i].MimoPs);
		//log_ERR("table.Entry[%d].AvgRssi0      : %d", i, table.Entry[i].AvgRssi0);
		//log_ERR("table.Entry[%d].AvgRssi1      : %d", i, table.Entry[i].AvgRssi1);
		//log_ERR("table.Entry[%d].AvgRssi2      : %d", i, table.Entry[i].AvgRssi2);
		//log_ERR("table.Entry[%d].ConnectedTime : %d", i, table.Entry[i].ConnectedTime);
		//log_ERR("table.Entry[%d].TxRate        : %d", i, table.Entry[i].TxRate);

		printf("{\"ssid\":\"%s\",",get_single(buf));
		printf("\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",",
				table.Entry[i].Addr[0], table.Entry[i].Addr[1],
				table.Entry[i].Addr[2], table.Entry[i].Addr[3],
				table.Entry[i].Addr[4], table.Entry[i].Addr[5]);
		//log_ERR("mac addr: %02x:%02x:%02x:%02x:%02x:%02x",
		//		table.Entry[i].Addr[0], table.Entry[i].Addr[1],
		//		table.Entry[i].Addr[2], table.Entry[i].Addr[3],
		//		table.Entry[i].Addr[4], table.Entry[i].Addr[5]);

		*ip=0;
		*host=0;
		convertHwAddrToStr(achHwAddr,(char*)table.Entry[i].Addr);
		fp = fopen("/tmp/dnsmasq.leases", "r");
		if (fp) {
			while (fgets(buf, sizeof(buf), fp) != NULL) {
				if( strcasestr(buf,achHwAddr) ) {
					if( sscanf(buf, "%lu %s %s %s", &expires, achHwAddr, ip, host) )
						break;
				}
			}
			fclose (fp);
		}

		if(!*ip) {
			fp = fopen("/proc/net/arp", "r");
			if (fp){
				while (fgets(buf,sizeof(buf),fp)) {
					if(!strcasestr(buf,achHwAddr))
						continue;
					p_Token=strtok(buf,"\t ");
					if(!p_Token)
						continue;
					strcpy((char*)ip,p_Token);
				}
				fclose(fp);
			}
		}

		/* base64-encode host name */
		{
			int len = strlen(host);
			char *base64_host = NULL;

			if (len && (base64_host = malloc(cdcs_base64_get_encoded_len(len)))
					&& cdcs_base64encode(host, len, base64_host, 0) > 0) {
				printf("\"host\":\"%s\",", base64_host);
			}
			else {
				printf("\"host\":\"\",");
			}
			if (base64_host) {
				free(base64_host);
			}
		}

		if(!*ip)
			strcpy((char*)ip,"Unknown");
		printf("\"ipaddr\":\"%s\",", ip);
		printf("\"rssi\":\"%d\",\"psm\":\"%s\",", (int)table.Entry[i].AvgRssi0, (table.Entry[i].Psm == 0)? "PWR_ACTIVE":"PWR_SAVE");
		uptime = table.Entry[i].ConnectedTime;
		upday = uptime / (24 * 3600);
		uphr = (uptime - upday * 24 * 3600) / (3600);
		upmin = (uptime - upday * 24 * 3600 - uphr * 3600) / 60;
		upsec = uptime - upday * 24 * 3600 - uphr * 3600 - upmin * 60;
		if (upday) {
			sprintf(uptimeStr, "%u Day", upday);
			if (upday > 1)
				strcat(uptimeStr, "s");
			strcat(uptimeStr, " ");
		}
		else {
			strcpy(uptimeStr, "");
		}
		printf("\"bw\":\"%s\",\"uptime\":\"%s%02u:%02u:%02u\"}",
			(table.Entry[i].TxRate.field.BW == 0)? "20M":"40M", uptimeStr, uphr, upmin, upsec);
	}
	printf("];");
	close(s);
	return 0;
}
#endif /* V_WIFI_backports */
#endif

#endif

void getDevInfo() {
	char rdb_var[128];
	char *pos;
	int count = 0;
	int i = 0;
	const char *strstr_fmt = "\t\"%s\":\"%s\",\n";
	const char *strstr_fmt_nc = "\t\"%s\":\"%s\"\n";
	const char *prefix = "link.profile.";

#define DUMP_STR(rdb_key, json_name) \
	pos = get_single_raw(rdb_key); \
	escape(pos, escapedStr); \
	printf(strstr_fmt, json_name, escapedStr);

	printf("{\n");

	DUMP_STR("system.product.sn", "sn");
	DUMP_STR("system.product.mac", "mac");
	DUMP_STR("wwan.0.imei", "imei");
	DUMP_STR("wwan.0.firmware_version","fw_ver");
	DUMP_STR("system.product.hwver","hw_ver");

	for(i = 1; i <= 6; ++i) {
		sprintf(rdb_var, "%s%u.enable", prefix, i);
		pos=get_single_raw(rdb_var);
		if(*pos && atoi(pos) == 1) {
			sprintf(rdb_var, "%s%u.apn", prefix, i);
			DUMP_STR(rdb_var,"apn");
			break;
		}
	}

	if(i == 7) {
		printf(strstr_fmt, "apn", "");
	}

	DUMP_STR("wwan.0.sim.raw_data.iccid","iccid");
	DUMP_STR("wwan.0.system_network_status.registered","network_registration_status");
	DUMP_STR("wwan.0.signal.0.rsrp","rsrp");
	DUMP_STR("wwan.0.currentband.current_selband","band");

	pos=get_single_raw("wwan.0.system_network_status.eci_pci_earfcn");
	count = 0;
	while(pos && *pos && count != 2) {
		if(*pos == ',') {
			count ++;
		}
		pos ++;
	}
	printf(strstr_fmt_nc, "earfcn", count == 2 ? pos : "");

	printf("}\n");
}

void getProfiles() {
#define	PREFIX	"link.profile."
int	i=0;
char cmd[128];
char *pos;
int def_profile=1;
	printf("var stpf=[");
	for(i=1; i<=6; ++i) {
		//find default profile
		sprintf(cmd, "%s%u.defaultroute", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos && atoi(pos)==1) {
			// check if the profile is enabled as well
			sprintf(cmd, "%s%u.enable", PREFIX, i);
			pos=get_single_raw(cmd);
			if(*pos && atoi(pos)==1) {
				def_profile=i;
			}
		}

		sprintf(cmd, "%s%u.name", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos==0)
			break;
		if( i>1 ) printf(",");
		escape(pos, escapedStr);
		printf("{\n\"name\":\"%s\",\n", escapedStr);

		printf("\"conntype\":");
		sprintf(cmd, "%s%u.conntype", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"\",");

		printf("\"enable\":");
		sprintf(cmd, "%s%u.enable", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			printf("\"%s\",\n", pos);
		}
		else {
			printf("\"0\",");
		}
		printf("\"profilenum\":\"%u\",\n",i);

		printf("\"dialnum\":");
		sprintf(cmd, "%s%u.dialstr", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"\",");

		printf("\"user\":");
		sprintf(cmd, "%s%u.user", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			escape(pos, escapedStr);
			printf("\"%s\",\n", escapedStr);
		}
		else
			printf("\"\",");

		printf("\"pass\":");
		sprintf(cmd, "%s%u.pass", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			escape(pos, escapedStr);
			printf("\"%s\",\n", escapedStr);
		}
		else
			printf("\"\",");

		printf("\"routes\":");
		sprintf(cmd, "%s%u.routes", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			escape(pos, escapedStr);
			printf("\"%s\",\n", escapedStr);
		}
		else
			printf("\"\",");

/*printf("{\n\"cmd\":\"%s\",\n", cmd);
printf("{\n\"pos\":\"%s\",\n", pos);
break;*/
		printf("\"snat\":");
		sprintf(cmd, "%s%u.snat", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

		printf("\"metric\":");
		sprintf(cmd, "%s%u.defaultroutemetric", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"1\",");

		printf("\"userpeerdns\":");
		sprintf(cmd, "%s%u.userpeerdns", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

		printf("\"readonly\":");
		sprintf(cmd, "%s%u.readonly", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

		printf("\"reconnect_delay\":");
		sprintf(cmd, "%s%u.reconnect_delay", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"30\",");

		printf("\"reconnect_retries\":");
		sprintf(cmd, "%s%u.reconnect_retries", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

		printf("\"authtype\":");
		sprintf(cmd, "%s%u.auth_type", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"none\",");

#if defined(PLATFORM_Serpent) || defined(PRODUCT_nwl200)
		printf("\"pdp_type\":");
		sprintf(cmd, "%s%u.pdp_type", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"ipv4\",");
#endif

#if (defined PLATFORM_Bovine)
/******************for PAD mode**********************************/
		printf("\"padmode\":");
		sprintf(cmd, "%s%u.pad_mode", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"tcp\",");

		printf("\"port\":");
		sprintf(cmd, "%s%u.pad_encode", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

		printf("\"host\":");
		sprintf(cmd, "%s%u.pad_host", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0.0.0.0\",");

		printf("\"pad_o\":");
		sprintf(cmd, "%s%u.pad_o", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

		printf("\"connection_op\":");
		sprintf(cmd, "%s%u.pad_connection_op", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"1\",");

		printf("\"tcp_nodelay\":");
		sprintf(cmd, "%s%u.tcp_nodelay", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");
/************************************************/
#endif
		printf("\"autoapn\":");
		sprintf(cmd, "%s%u.autoapn", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"0\",");

	// <START> To support perferred WWAN IP
		printf("\"preferredIPEnable\":");
		sprintf(cmd, "%s%u.preferred_ip.enable", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			escape(pos, escapedStr);
			printf("\"%s\",\n", escapedStr);
		}
		else
			printf("\"\",");

		printf("\"preferredIPAddr\":");
		sprintf(cmd, "%s%u.preferred_ip.addr", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			escape(pos, escapedStr);
			printf("\"%s\",\n", escapedStr);
		}
		else
			printf("\"\",");
	// < END > To support perferred WWAN IP

		printf("\"APNName\":");
		sprintf(cmd, "%s%u.apn", PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			escape(pos, escapedStr);
			printf("\"%s\"\n}", escapedStr);
		}
		else
			printf("\"\"\n}");
	}
	printf("];var def_profile=%u;\n",def_profile);
}

void getVLANs() {
#define	VLAN_PROFILE_PREFIX	"vlan."
	int	i=0;
	char cmd[128];
	char buf[512];
	char *pos;

	pos = get_single("vlan.count");
	int vlan_count = strtol(pos, 0, 10);

	printf("var vlan=[");
	for(i=0; i<vlan_count; ++i) {
		if( i>0 ) printf(",");

		printf("{\n\"name\":");
		sprintf(cmd, "%s%u.name", VLAN_PROFILE_PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
		{
			escape(pos, escapedStr);
			printf("\"%s\",\n", escapedStr);
		}
		else
			printf("\"\",");

		printf("\"address\":");
		sprintf(cmd, "%s%u.address", VLAN_PROFILE_PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"\",");

		printf("\"netmask\":");
		sprintf(cmd, "%s%u.netmask", VLAN_PROFILE_PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\",\n", pos);
		else
			printf("\"\",");

		printf("\"status\":");
		sprintf(cmd, "%s%u.status", VLAN_PROFILE_PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos) {
			lan_str(pos, buf);
			printf("\"%s\",\n", buf);
		}
		else
			printf("\"\",");

		printf("\"vlanid\":");
		sprintf(cmd, "%s%u.vlanid", VLAN_PROFILE_PREFIX, i);
		pos=get_single_raw(cmd);
		if(*pos)
			printf("\"%s\"\n}", pos);
		else
			printf("\"\"\n}");
	}

	printf("];\n");
}

int main(int argc, char* argv[], char* envp[]) {
	char pathname[512];
	char *cp;

	/* set path first for executable files under /usr/local folder */
	if ((cp = getenv("PATH")) != 0) {
	  sprintf(pathname, "/usr/local/bin:/usr/local/sbin:%s", getenv("PATH"));
	  (void)setenv("PATH", pathname, 1);
	}

	//syslog(LOG_ERR,"ajax: start");

	//system( "log \"Starting Ajax.cgi (logcat)\n\"" );

	printf("Content-Type: text/html \r\n\r\n");
	if(rdb_start()) {
		syslog(LOG_ERR,"can't open cdcs_DD %i ( %s )", -errno, strerror(errno));
		return -1;
	}

#if ((defined PLATFORM_Platypus2) || (defined PLATFORM_Bovine)) && ( !defined MODE_recovery )
	char *p_str, *p_str2;
#if (defined PLATFORM_Bovine)
	p_str=getenv("SESSION_ID");
	p_str2=getenv("sessionid");
	if( p_str==0 || p_str2==0 || strcmp(p_str, p_str2) ) {
		exit(0);
	}
#endif
	p_str = getenv("QUERY_STRING");
	if (p_str && strcmp(p_str,"wlInfo")==0) {
		wlInfo();
	}
	else if (p_str && strcmp(p_str,"updateWPS")==0) {
		updateWPS();
		rdb_end();
		return 0;
	}
	else if (p_str && strcmp(p_str,"getProfiles")==0) {
		getProfiles();
		rdb_end();
		return 0;
	}
	else if (p_str && strcmp(p_str,"getVLANs")==0) {
		getVLANs();
		rdb_end();
		return 0;
	}
	else {
		status_refresh();
  #ifdef BOARD_nhd1w
		printf("var mhs_docked='%s';", get_single("wwan.0.mhs.docked") );
		printf("var mhs_chargingonly='%s';", get_single("wwan.0.mhs.chargingonlymode") );
		printf("var mhs_operationmode='%s';", get_single("mhs.operationmode") );
		printf("var pdp_stat='%s';", get_single("wwan.0.system_network_status.pdp0_stat") );
  #endif
	}
#elif (defined PLATFORM_Serpent)
	// This is just a placeholder to make something show up on Serpent/Fisher APN profile page.
	char* p_str;
	p_str = getenv("QUERY_STRING");
	if (p_str && strcmp(p_str,"getProfiles")==0) {
		getProfiles();
		rdb_end();
		return 0;
	}
	else if (p_str && strcmp(p_str,"getDevInfo")==0) {
		getDevInfo();
		rdb_end();
		return 0;
	}
	else {
		status_refresh();
	}
#else
	status_refresh();
#endif

#if (defined PLATFORM_Platypus2)
	print_wan_info();
#endif

	printf("\n");
	rdb_end();
	//syslog(LOG_ERR,"ajax: done");
	return 0;
}
