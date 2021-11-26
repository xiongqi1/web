#define _GNU_SOURCE

#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include "rdb_ops.h"

#define OPMODE_SCRIPT "opmode_change.sh"
#define RDB_BUF_SIZE	512

enum operation_mode_t {operation_simple,operation_soho,operation_advance};

static int running=1;
static int force_trigger=0;
static char rdb_buf[RDB_BUF_SIZE];

// local functions
int clone_mhs_wifi(enum operation_mode_t mode, int previous,int wifi);
const char* rdb_get_str(const char* var);


void sig_handler(int signo)
{
	switch(signo) {
		case SIGTERM:
			syslog(LOG_ERR,"got SIGTERM");
			running=0;
			break;
			
		case SIGHUP:
			syslog(LOG_ERR,"got SIGHUP");
			force_trigger=1;
			
			break;
			
		default:
			syslog(LOG_ERR,"unknown sig detected - %d",signo);
			break;
	}
}

#define TRIGGER_COUNT	3
#define TRIGGER_LENGTH	16

const char* triggers[TRIGGER_COUNT]={ 
	"mhs.operationmode", 
	"wwan.0.mhs.chargingonlymode",
	"atmgr.status"
};

const char* rdb_variables[]={
	"wwan.0.mhs.docked",
 	0
};

char cur_triggers[TRIGGER_COUNT][TRIGGER_LENGTH]={ {0,}, };
char prev_triggers[TRIGGER_COUNT][TRIGGER_LENGTH]={ {0,}, };
		
int subscribe_variable(const char* rdb)
{
	// subscribe activated
	syslog(LOG_INFO,"subscribing %s",rdb);
	if(rdb_subscribe_variable(rdb)<0) {
		
		// try again after creating
		syslog(LOG_INFO,"creating %s", rdb);
		rdb_create_variable(rdb, "", CREATE, ALL_PERM, 0, 0);
		
		if(rdb_subscribe_variable(rdb)<0) {
			syslog(LOG_ERR,"failed to subscribe %s - %s",rdb,strerror(errno));
			return -1;
		}
	}
	
	return 0;
}

int create_rdb() 
{
	int i;
	
	i=0;
	while(rdb_variables[i]) {
		rdb_create_variable(rdb_variables[i], "", CREATE, ALL_PERM, 0, 0);
		
		if(errno!=EEXIST)
			syslog(LOG_ERR,"failed to create %s - %s",rdb_variables[i],strerror(errno));
		
		i++;
	}
	
	return 0;
}

int subscribe_rdb(void)
{
	int i;
	
	for(i=0;i<TRIGGER_COUNT;i++)
		subscribe_variable(triggers[i]);
	
	return 0;
}

void update_prev_triggers(void)
{
	int i;
	
	// compare to prev
	for(i=0;i<TRIGGER_COUNT;i++)
		strcpy(prev_triggers[i],cur_triggers[i]);
}

void clear_prev_triggers(void)
{
	int i;
	
	// compare to prev
	for(i=0;i<TRIGGER_COUNT;i++)
		prev_triggers[i][0]=0;
}


void update_cur_triggers(void)
{
	int i;
	
	// get cur triggers
	for(i=0;i<TRIGGER_COUNT;i++) {
		if(rdb_get_single(triggers[i],cur_triggers[i],TRIGGER_LENGTH)<0) {
			syslog(LOG_ERR,"failed to read %s - %s",triggers[i],strerror(errno));
		}
	}
	
}

int is_any_changed(void) 
{
	int i;
	
	int chg=0;
	
	// compare to prev
	for(i=0;i<TRIGGER_COUNT;i++) {
		if(strcmp(cur_triggers[i],prev_triggers[i])) {
			chg=1;
		}
	}
	
	return chg;
}

int rdb_set_single_if_chg_sub(const char* rdb,const char* val,int* chg)
{
	const char* oldval;
	int ifchg;
	
	// read oldval
	oldval=rdb_get_str(rdb);
	
	ifchg=!oldval || strcmp(val,oldval);
	
	if(chg)
		*chg=ifchg;
	
	// if changed
	if(ifchg)
		return rdb_set_single(rdb,val);
	
	return 0;
}

int rdb_set_single_if_chg(const char* rdb,const char* val)
{
	return rdb_set_single_if_chg_sub(rdb,val,0);
}

int rdb_rewrite_single(const char* rdb)
{
	const char* oldval;
	
	// read oldval
	oldval=rdb_get_str(rdb);
	if(oldval) {
		oldval=strdupa(oldval);
		return rdb_set_single(rdb,oldval);
	}
	
	return 0;
}

int control_mhs_dock()
{
	const char* rdb_en;
	
	syslog(LOG_INFO,"### sending docking command to MHS ");
	rdb_en="dock";
	
	rdb_set_single("wwan.0.mhs.command",rdb_en);
	
	return 0;
}

int control_eth_phy(int on)
{
	const char* eth_block;
	
	eth_block=on?"0":"1";
	
	syslog(LOG_INFO,"### sending eth block command to the template - %s",eth_block);
	rdb_set_single_if_chg("link.profile.0.mhs_eth_block",eth_block);
	
	return 0;
}


int control_mhs_wifi(int en)
{
	const char* rdb_en;
	
	if(en) {
		syslog(LOG_INFO,"### turning on MHS wifi");
		rdb_en="enablewlan";
	}
	else {
		syslog(LOG_INFO,"### turning off MHS wifi");
		rdb_en="disablewlan";
	}
	
	rdb_set_single("wwan.0.mhs.command",rdb_en);
	
	return 0;
}


int control_cradle_wifi(int en)
{
	const char* rdb_en;
	
	if(en) {
		syslog(LOG_INFO,"### turning on Cradle wifi");
		rdb_en="1";
	}
	else {
		syslog(LOG_INFO,"### turning off Cradle wifi");
		rdb_en="0";
	}
	
	return rdb_set_single_if_chg("wlan.0.enable",rdb_en);
}

int read_rdb_to_buf(const char* src,char* dst,int (*convert)(const char* src,char* val,int size))
{
	// read src
	if( rdb_get_single(src,rdb_buf,sizeof(rdb_buf)) <0 ) {
		syslog(LOG_ERR,"failed to read %s - %s",src,strerror(errno));
		return -1;
	}
	
	// convert
	if(convert && convert(src,rdb_buf,sizeof(rdb_buf))<0) {
		syslog(LOG_ERR,"failed to convert src=%s,val=%s",src,rdb_buf);
		return -1;
	}
	
	strcpy(dst,rdb_buf);
	
	return 0;
}

int convert_wifi_encrypt(const char* src,char* val,int size)
{
	int idx;
	char buf[RDB_BUF_SIZE];
	
	
	if( sscanf(val,"%d %s",&idx,buf) != 2) {
		syslog(LOG_ERR,"incorrect wifi encryption format - %s",val);
		return -1;
	}
	
	const char* rt_wifi;
	
	switch(idx) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			rt_wifi="OPEN";
			break;
		
		case 5:
		case 6:
			rt_wifi="WPAPSK";
			break;
			
		case 7:
		case 8:
			rt_wifi="WPA2PSK";
			break;
			
		case 9:
			rt_wifi="WPAPSKWPA2PSK";
			break;
			
			
		default:
			syslog(LOG_ERR,"incorrect wifi encryption index - val=%s,idx=%d",val,idx);
			return -1;
	}
	
	syslog(LOG_INFO,"converting %s(%s) to (%s)",src,val,rt_wifi);
	
	strcpy(val,rt_wifi);
	
	return 0;
}

int convert_wifi_encrypt2(const char* src,char* val,int size)
{
	int idx;
	char buf[RDB_BUF_SIZE];
	
	
	if( sscanf(val,"%d %s",&idx,buf) != 2) {
		syslog(LOG_ERR,"incorrect wifi encryption format - %s",val);
		return -1;
	}
	
	const char* rt_wifi;
	
	switch(idx) {
		case 0:
			rt_wifi="NONE";
			break;
			
		case 1:
		case 2:
		case 3:
		case 4:
			rt_wifi="WEP";
			break;
		
		case 5:
		case 7:
			rt_wifi="TKIP";
			break;
			
		case 9:
			rt_wifi="TKIPAES";
			break;
			
			
		case 6:
		case 8:
			rt_wifi="AES";
			break;
			
		default:
			syslog(LOG_ERR,"incorrect wifi encryption index - val=%s,idx=%d",val,idx);
			return -1;
	}
	
	syslog(LOG_INFO,"converting %s(%s) to (%s)",src,val,rt_wifi);
	
	strcpy(val,rt_wifi);
	
	return 0;
}

int convert_print(const char* src,char* val,int size)
{
	syslog(LOG_INFO,"converting %s(%s) to (%s)",src,val,val);
	
	return 0;
}

/*
int enable_mhs_route(int enable,const char* mhs_ipaddr)
{
	char cmd[128];
	
	if(enable) {
		syslog(LOG_INFO,"adding host %s to wwan0",mhs_ipaddr);
		snprintf(cmd,sizeof(cmd),"route add -host %s dev wwan0 2>&1 | logger -t opmode",mhs_ipaddr);
		
		syslog(LOG_INFO,"enabling proxy arp - pseudo bridging");
		system("echo 1 > /proc/sys/net/ipv4/conf/br0/proxy_arp");
	}
	else {
		syslog(LOG_INFO,"remove host %s to wwan0",mhs_ipaddr);
		snprintf(cmd,sizeof(cmd),"route delete -host %s dev wwan0 2>&1 | logger -t opmode",mhs_ipaddr);
		
		syslog(LOG_INFO,"disabling proxy arp - pseudo bridging");
		system("echo 0 > /proc/sys/net/ipv4/conf/br0/proxy_arp");
	}
}
*/

int convert_wifi_ssid(const char* src,char* val,int size)
{
	char* ptr;
	int val_len;
	
	char cssid[RDB_BUF_SIZE];
	
	const char* suffix="-C";
	
	strcpy(cssid,val);
	
	// add suffix
	val_len=strlen(cssid);
	if(val_len<18) {
		ptr=cssid+val_len;
	}
	else{
		ptr=cssid+18;
		
		if(!strncmp(suffix,ptr,2))
			suffix="_C";
	}
	strcpy(ptr,suffix);
	
	syslog(LOG_INFO,"converting %s(%s) to (%s)",src,val,cssid);
	
	strcpy(val,cssid);
	
	return 0;
}

const char* rdb_get_bufstr(const char* var,char* dst,int dstlen)
{
	static char val[RDB_BUF_SIZE];
	
	if(!dst) {
		dst=val;
		dstlen=sizeof(val);
	}
	
	if(rdb_get_single(var,dst,dstlen)<0) {
		syslog(LOG_ERR,"failed to read %s - %s",var,strerror(errno));
		return 0;
	}
	
	if(!dst)
		return val;
	
	return dst;
}

const char* rdb_get_str(const char* var)
{
	return rdb_get_bufstr(var,0,0);
}

static unsigned  __inet_aton(const char* addr)
{
	return ntohl(inet_addr(addr));
}

const char* __inet_ntoa(unsigned int addr)
{
	struct in_addr in;
	
	in.s_addr=htonl(addr);
	
	return inet_ntoa(in);
}


int setup_ebtables(int pretend_to_be_mhs)
{
	char cmd[1024];
	
	snprintf(cmd,sizeof(cmd),"ebtables_mhs.sh %d",pretend_to_be_mhs?1:0);
	syslog(LOG_INFO,"launching ebtables_mhs - %s",cmd);
	system(cmd);
	
	return 0;
}
		
int setup_mhs_alias(int enable)
{
	const char* mhs_ipaddr;
	char ifconfig_cmd[256];
	const char* alias_name="br0:1";
	
	
	if(enable) {
		syslog(LOG_INFO,"creating alias for MHS");
		// get mhs address
		mhs_ipaddr=rdb_get_str("link.profile.0.mhs_address");
		if(!mhs_ipaddr || !mhs_ipaddr[0]) {
			syslog(LOG_ERR,"failed to read link.profile.0.mhs_address");
			return -1;
		}
						
		// build bridge ifconfig
		snprintf(ifconfig_cmd,sizeof(ifconfig_cmd),"ifconfig %s %s netmask 255.255.255.255 2> /dev/null",alias_name,mhs_ipaddr);
	}
	else {
		syslog(LOG_INFO,"deleting alias for MHS");
		snprintf(ifconfig_cmd,sizeof(ifconfig_cmd),"ifconfig %s down > /dev/null",alias_name);
	}
	
	if(system(ifconfig_cmd)<0) {
		syslog(LOG_ERR,"cmd failed (%s) - %s",ifconfig_cmd,strerror(errno));
		return -1;
	}
	
	return 0;
}

unsigned get_current_lan_ipaddr()
{
	const char* val;
	
	// get ip
	if((val=rdb_get_str("link.profile.0.address"))==0 || !val[0]) {
		syslog(LOG_INFO,"not valid value in link.profile.0.address");
		return ~0;
	}
	
	return __inet_aton(val);
}

struct rdb_to_rdb {
	const char* src;
	const char* dst;
};

int copy_rdb_to_rdb(const struct rdb_to_rdb* rdbs,int backup)
{
	const char* src_val;
	int i;
	
	const char* src_var;
	const char* dst_var;
	
	char src_val_buf[RDB_BUF_SIZE];
	
	for(i=0;rdbs[i].src && rdbs[i].dst;i++) {
		
		if(backup) {
			src_var=rdbs[i].src;
			dst_var=rdbs[i].dst;
		}
		else {
			src_var=rdbs[i].dst;
			dst_var=rdbs[i].src;
		}
			
		// get source value
		src_val=rdb_get_str(src_var);
		if(!src_val)
			src_val="";
		
		strcpy(src_val_buf,src_val);
		
		syslog(LOG_INFO,"copying %s to %s - '%s'",src_var,dst_var,src_val);
		
		// put the value to the destination
		rdb_set_single_if_chg(dst_var,src_val_buf);
	}
	
	return 0;
}

int get_netif_info_raw(int fd,const char* if_name,int cmd,char* buf,int buf_len)
{
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

int isifup(const char* if_name)
{
	struct ifreq ifr;
	int sockfd;
	
	int stat=0;

	// NOTE - that this socket is just created to be used as a hook in to the socket layer
	// for the ioctl call.
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd >= 0) {
		strcpy(ifr.ifr_name, if_name);

		if (ioctl(sockfd, SIOCGIFINDEX, &ifr) >= 0) {
			if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) >= 0) {
				//syslog(LOG_INFO,"flags = 0x%08x",ifr.ifr_flags);
				if (ifr.ifr_flags &IFF_UP) {
					stat=1;
				}
				else {
					stat=0;
				}
			}
			else {
				//syslog(LOG_ERR,"failed to send ioctrl(SIOCGIFFLAGS) - %s",strerror(errno));
			}
		}
		else {
			//syslog(LOG_ERR,"failed to send ioctrl(SIOCGIFINDEX) - %s",strerror(errno));
		}
	}
	else {
		//syslog(LOG_ERR,"failed to open socket - %s",strerror(errno));
	}

	close(sockfd);
	return stat;
}

int wait_until_netif_applied(int delay)
{
	const char* if_name="br0";
	int fd=-1;
	
	char ipaddr[32];
	char netmask[32];
	
	const char* cfg_ipaddr;
	const char* cfg_netmask;
	

	int i;
	
	
	// get configuration ip address
	cfg_ipaddr=rdb_get_str("link.profile.0.address");
	if(cfg_ipaddr)
		cfg_ipaddr=strdupa(cfg_ipaddr);
	
	// get configuration netmask
	cfg_netmask=rdb_get_str("link.profile.0.netmask");
	if(cfg_netmask)
		cfg_netmask=strdupa(cfg_netmask);
	
	syslog(LOG_INFO,"rdb address='%s',netmask='%s'",cfg_ipaddr?cfg_ipaddr:"",cfg_netmask?cfg_netmask:"");
	
	// open sock dgram
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd<0) {
		syslog(LOG_ERR,"failed to open SOCK_DGRAM - %s",strerror(errno));
		goto err;
	}
	
	for(i=0;i<delay;i++) {
		// get ip address
		if( (get_netif_info_raw(fd, if_name, SIOCGIFADDR,ipaddr,sizeof(ipaddr))) < 0 ) {
			syslog(LOG_ERR,"failed to get IP address");
			goto err;
		}
	
		// get netmask
		if( (get_netif_info_raw(fd, if_name, SIOCGIFNETMASK,netmask,sizeof(netmask))) < 0 ) {
			syslog(LOG_ERR,"failed to get netmask");
			goto err;
		}
		
		syslog(LOG_INFO,"current ipaddr='%s',netmask='%s'",ipaddr,netmask);
		
		if(!strcmp(ipaddr,cfg_ipaddr) && !strcmp(netmask,cfg_netmask))
			goto succ;
		
		if(sleep(1)!=0) {
			syslog(LOG_ERR,"punk in sleep()!");
			break;
		}
	}
	
err:	
	if(fd>0)	
		close(fd);
		
	return -1;
	
succ:
	if(fd>0)
		close(fd);
	return 0;
}

int clone_mhs_wifi_sub(enum operation_mode_t mode, int previous)
{
	int err;
	
	err=0;
	
/*
	* Ralink configuration
	
	wlan.0.conf.54g_rate
	wlan.0.conf.basic_rate
	wlan.0.conf.preamble_type
	wlan.0.conf.transmit_power
	wlan.0.conf.fragmentation_threshold
	wlan.0.conf.rts_threshold
	wlan.0.conf.beacon_interval
	wlan.0.conf.dtim_interval
	wlan.0.conf.channel
	wlan.0.ssid
	wlan.0.country
	wlan.1.ssid
	wlan.2.ssid
	wlan.3.ssid
	wlan.0.network_key
	wlan.0.encryption_strength
	wlan.0.network_key1
	wlan.0.network_key2
	wlan.0.network_key3
	wlan.0.network_key4
	wlan.0.wpa_pre_shared_key
	wlan.0.radius_server_ip
	wlan.0.radius_port
	wlan.0.radius_key
	wlan.0.encryption_type
	wlan.0.wpa_group_rekey_interval
	wlan.0.wpa2_preauthentication
	wlan.0.wep_encryption
	wlan.0.network_auth
	wlan.0.bg_protection
	wlan.0.wds_mode
	wlan.0.wds_maclist
	wlan.0.wds_key
	wlan.0.tx_burst
	wlan.0.pkt_aggregate
	wlan.0.hide_accesspoint
	wlan.0.conf.bandwidth
	
	* Sierra configuration
	
	wwan.0.mhs.hswlanchan 0
	wwan.0.mhs.hswlanssid AC754S-0B7A
	wwan.0.mhs.hscustlastwifichan 11
	wwan.0.mhs.hswlanhiddenssid 0
	wwan.0.mhs.hsrtrsecdns 0x0
	wwan.0.mhs.hsrtrpridns 0xC0A80101
	wwan.0.mhs.hsrtrdhcpena 1
	wwan.0.mhs.hsrtriphi 0xC0A80163
	wwan.0.mhs.hsrtriplow 0xC0A80102
	wwan.0.mhs.hswlanencrip 8 WPA2 CCMP
	wwan.0.mhs.hswlanpassphr 61285967
	wwan.0.mhs.hswlanphymode 4 WLAN 802.11BGN
	wwan.0.mhs.hswlanprotect 
	wwan.0.mhs.hswlanip 0xC0A80101	
	
*/			
	unsigned dhcp_range_lo;
	unsigned dhcp_range_hi;
	unsigned wlan_ipaddr;
	unsigned wlan_ipaddr2=0;
	unsigned mhs_ipaddr;
	unsigned wlan_nwmask;
	unsigned dhcp_gateway=0;
	
	unsigned wlan_ipaddr_twin;
	unsigned wlan_ipaddr_sibling;
	
	unsigned mhs_range_lo;
	unsigned mhs_range_hi;
	
	const char* val;
	
	int stat=0;
	
	const char* mhs_hswlanssid;
	const char* mhs_hswlanpassphr;
	const char* mhs_hswlanencrip;
	const char* mhs_encryption_type;
	
	unsigned inc;
	unsigned int lastaddr;
	unsigned int startaddr;
	
	syslog(LOG_INFO,"extracting network from MHS");
	
	if(previous) {
		char lohi[RDB_BUF_SIZE];
		char* ptr;
		
		
		// get previous range
		val=rdb_get_str("link.profile.0.mhs_range.0");
		if(!val || !val[0]) {
			syslog(LOG_ERR,"previous configuration not found - link.profile.0.mhs_range.0");
			return -1;
		}
		
		// clone range
		strncpy(lohi,val,sizeof(lohi));
		lohi[sizeof(lohi)-1]=0;
		
		// search delimiter
		ptr=strchr(lohi,',');
		if(!ptr) {
			syslog(LOG_ERR,"incorrect format in link.profile.0.mhs_range.0");
			return -1;
		}
		
		*ptr++=0;
		dhcp_range_lo=__inet_aton(lohi);
		dhcp_range_hi=__inet_aton(ptr);
		
		// get previous address
		val=rdb_get_str("link.profile.0.mhs_address");
		if(!val | !val[0]) {
			syslog(LOG_ERR,"previous configuration not found - link.profile.0.mhs_address");
			return -1;
		}
		wlan_ipaddr=__inet_aton(val);
		
		
		// get previous netmask
		val=rdb_get_str("link.profile.0.mhs_netmask");
		if(!val | !val[0]) {
			syslog(LOG_ERR,"previous configuration not found - link.profile.0.mhs_netmask");
			return -1;
		}
		wlan_nwmask=__inet_aton(val);
	}
	else {
		// get dhcp lo
		if((val=rdb_get_str("wwan.0.mhs.hsrtriplow"))==0 || !val[0]) {
			syslog(LOG_INFO,"not valid value in wwan.0.mhs.hsrtriplow");
			return -1;
		}
		
		dhcp_range_lo=__inet_aton(val);
		
		// get dhcp hi
		if((val=rdb_get_str("wwan.0.mhs.hsrtriphi"))==0 || !val[0]) {
			syslog(LOG_INFO,"not valid value in wwan.0.mhs.hsrtriphi");
			return -1;
		}
		dhcp_range_hi=__inet_aton(val);
		
		// get ip
		if((val=rdb_get_str("wwan.0.mhs.hswlanip"))==0 || !val[0]) {
			syslog(LOG_INFO,"not valid value in wwan.0.mhs.hswlanip");
			return -1;
		}
		
		wlan_ipaddr=__inet_aton(val);
		
		// get mask - hswlanmask
		if((val=rdb_get_str("wwan.0.mhs.hsrtrnetmask"))==0 || !val[0]) {
			syslog(LOG_INFO,"not valid value in wwan.0.mhs.hsrtrnetmask");
			return -1;
		}
		wlan_nwmask=__inet_aton(val);
	}
	
	mhs_ipaddr=wlan_ipaddr;
	mhs_range_lo=dhcp_range_lo;
	mhs_range_hi=dhcp_range_hi;
	
	syslog(LOG_INFO,"### MHS : ip addr =%s",__inet_ntoa(wlan_ipaddr));
	syslog(LOG_INFO,"### MHS : dhcp_lo =%s",__inet_ntoa(dhcp_range_lo));
	syslog(LOG_INFO,"### MHS : idhcp_hi=%s",__inet_ntoa(dhcp_range_hi));
	syslog(LOG_INFO,"### MHS : nw mask=%s",__inet_ntoa(wlan_nwmask));
	

	// get the twin IP address
	lastaddr=((wlan_ipaddr & wlan_nwmask) | (~wlan_nwmask))-1;
	startaddr=((wlan_ipaddr & wlan_nwmask))+1;
	// take either first or last as lan ip address
	if(startaddr == wlan_ipaddr)
		wlan_ipaddr_twin=lastaddr;
	else
		wlan_ipaddr_twin=startaddr;
	syslog(LOG_INFO,"twin IP address = %s",__inet_ntoa(wlan_ipaddr_twin));
	
	// get the sibiling IP address
	inc=(~wlan_nwmask+1);
	wlan_ipaddr_sibling=wlan_ipaddr+inc;
	syslog(LOG_INFO,"sibiling IP address = %s",__inet_ntoa(wlan_ipaddr_sibling));
	
	// get wlan ip address
	switch(mode) {
		case operation_advance:
		case operation_soho: 
			inc=(~wlan_nwmask+1);
			syslog(LOG_INFO,"### applying sibiling network - adding %s",__inet_ntoa(inc));
			
			wlan_ipaddr=wlan_ipaddr_sibling;
			wlan_ipaddr2=wlan_ipaddr_twin;
			dhcp_range_lo+=inc;
			dhcp_range_hi+=inc;
			
			// use cradle ip address as gateway
			dhcp_gateway=wlan_ipaddr;
			break;
			
		case operation_simple:
			inc=0;
			syslog(LOG_INFO,"### cloning network");
			
			// check if network is big enough
			if(~wlan_nwmask<7) {
				syslog(LOG_ERR,"MHS network range is too small for simple mode - minimum 29 bit netmask (255.255.255.248) - cur=%s",__inet_ntoa(wlan_nwmask));
				return -1;
			}
					
			// use MHS as gateway for the clients from MHS
			dhcp_gateway=wlan_ipaddr;
			
			wlan_ipaddr=wlan_ipaddr_twin;
			wlan_ipaddr2=wlan_ipaddr_sibling;
			break;
	}
	
	syslog(LOG_INFO,"### Cradle : ip addr =%s",__inet_ntoa(wlan_ipaddr));
	syslog(LOG_INFO,"### Cradle : dhcp_lo =%s",__inet_ntoa(dhcp_range_lo));
	syslog(LOG_INFO,"### Cradle : idhcp_hi=%s",__inet_ntoa(dhcp_range_hi));
	syslog(LOG_INFO,"### Cradle : nw mask=%s",__inet_ntoa(wlan_nwmask));
	syslog(LOG_INFO,"### Cradle : dhcp gateway=%s",__inet_ntoa(dhcp_gateway));
	
	char lo[RDB_BUF_SIZE];
	char hi[RDB_BUF_SIZE];
	
	syslog(LOG_INFO,"extracting wifi configuration from MHS");
	
	char ssid[RDB_BUF_SIZE];
	char wpa_key[RDB_BUF_SIZE];
	char auth[RDB_BUF_SIZE];
	char enc_type[RDB_BUF_SIZE];
	
	const char* mhs_ssid;
	
	struct rdb_to_rdb rdbs[]={
		{"wwan.0.mhs.hswlanssid",    "link.profile.0.mhs_hswlanssid"},
		{"wwan.0.mhs.hswlanpassphr", "link.profile.0.mhs_hswlanpassphr"},
		{"wwan.0.mhs.hswlanencrip",  "link.profile.0.mhs_hswlanencrip"},
		{"wlan.0.encryption_type",   "link.profile.0.mhs_encryption_type"},
		{0,0}
	};
	
	// backup if not taking previous configuraiton
	if(!previous) {
		syslog(LOG_INFO,"backup current mhs information");
		copy_rdb_to_rdb(rdbs,1);
	}
	else {
		syslog(LOG_INFO,"using previous mhs information");
	}
	
	// switch the rdb variables if previous
	mhs_hswlanssid     =previous?rdbs[0].dst:rdbs[0].src;
	mhs_hswlanpassphr  =previous?rdbs[1].dst:rdbs[1].src;
	mhs_hswlanencrip   =previous?rdbs[2].dst:rdbs[2].src;
	mhs_encryption_type=previous?rdbs[3].dst:rdbs[3].src;
	
	mhs_ssid=rdb_get_str(mhs_hswlanssid);
	
	if(!mhs_ssid || !*mhs_ssid) {
		syslog(LOG_ERR,"mhs wifi configuration does not exist");
		
		stat=-1;
	}
	else {
		syslog(LOG_INFO,"mhs wifi configuration exist");
		
		switch(mode) {
			
			case operation_simple:
				syslog(LOG_INFO,"### cloning MHS configuraiton...");
				if(read_rdb_to_buf(mhs_hswlanssid,ssid,convert_print)<0 || !ssid[0])
					return -1;
				break;
		
			case operation_advance:
			case operation_soho:
				syslog(LOG_INFO,"### making a sibiling configuration of MHS...");
				if(read_rdb_to_buf(mhs_hswlanssid,ssid,convert_wifi_ssid)<0 || !ssid[0])
					return -1;
				break;
		}
	
		if(read_rdb_to_buf(mhs_hswlanpassphr,wpa_key,convert_print)<0 || !wpa_key[0])
			return -1;
		if(read_rdb_to_buf(mhs_hswlanencrip,auth,convert_wifi_encrypt)<0 || !auth[0])
			return -1;
		if(read_rdb_to_buf(mhs_hswlanencrip,enc_type,convert_wifi_encrypt2)<0 || !enc_type[0]) {
			return -1;
		}
	
		//syslog(LOG_ERR,"TODO: copy more configuraton");
	
		syslog(LOG_ERR,"applying wifi configuration");
		
		// TODO: get the WEP password and use it rather than the empty password - currently Sierra does not have any AT command for WEP password
/*		
		if(!strcmp(auth,"OPEN")) {
			rdb_set_single_if_chg("wlan.0.network_key_id","1");
			rdb_set_single_if_chg("wlan.0.network_key1","");
		}
*/		
		
		rdb_set_single_if_chg("wlan.0.ssid",ssid);
		rdb_set_single_if_chg("wlan.0.wpa_pre_shared_key",wpa_key);
		rdb_set_single_if_chg("wlan.0.network_auth",auth);
		rdb_set_single_if_chg("wlan.0.encryption_type",enc_type);
	}
	
	syslog(LOG_ERR,"applying ethernet configuration");
	
	// store mhs information
	rdb_set_single("link.profile.0.mhs_address",__inet_ntoa(mhs_ipaddr));
	rdb_set_single("link.profile.0.mhs_netmask",__inet_ntoa(wlan_nwmask));
	
	// range
	strcpy(lo,__inet_ntoa(mhs_range_lo));
	strcpy(hi,__inet_ntoa(mhs_range_hi));
	snprintf(rdb_buf,sizeof(rdb_buf),"%s,%s",lo,hi);
	rdb_set_single("link.profile.0.mhs_range.0",rdb_buf);
	
	rdb_database_lock(0);
	
	// get dhcp range
	strcpy(lo,__inet_ntoa(dhcp_range_lo));
	strcpy(hi,__inet_ntoa(dhcp_range_hi));
	snprintf(rdb_buf,sizeof(rdb_buf),"%s,%s",lo,hi);
	rdb_set_single_if_chg("service.dhcp.range.0",rdb_buf);
	
	// set dhcp
	rdb_set_single_if_chg("service.dhcp.gateway.0",__inet_ntoa(dhcp_gateway));
	rdb_set_single_if_chg("service.dhcp.dns1.0",__inet_ntoa(dhcp_gateway));
	rdb_set_single_if_chg("service.dhcp.dns2.0","");
	
	// ip addr & mask
	rdb_set_single_if_chg("link.profile.0.address2",__inet_ntoa(wlan_ipaddr2));
	rdb_set_single_if_chg("link.profile.0.address",__inet_ntoa(wlan_ipaddr));
	rdb_set_single_if_chg("link.profile.0.netmask",__inet_ntoa(wlan_nwmask));
	
	rdb_database_unlock();
	
	return stat;
}

int clone_mhs_wifi(enum operation_mode_t mode, int previous,int wifi)
{
	int i;
	int stat;
	int wifi_en;
	
	stat=clone_mhs_wifi_sub(mode,previous);
	
	wifi_en=(stat>=0) && wifi;
	
	control_cradle_wifi(wifi_en);
	
	i=0;
	do {
		i++;
		
		// wait for 5 seconds until the interface gets the new ip address
		if(wait_until_netif_applied(10)>=0) {
			syslog(LOG_INFO,"clone applied");
			break;
		}
		
		// rewrite address
		syslog(LOG_ERR,"br0 does not have correct configuration. trying again #%d",i);
		rdb_rewrite_single("link.profile.0.address");
		
	} while(i<3);
	
	
	i=0;
	do {
		i++;
		
		if(!wifi_en)
			break;
		
		if(isifup("ra0")) {
			//sleep(3);
			break;
		}
		
		syslog(LOG_INFO,"waiting until ra0 is up #%d",i);
		sleep(1);
		
	} while(i<10);
	
	return 0;
}

int set_mhs_cloned_status(int stat)
{
	return rdb_set_single("mhs.cloned",stat?"1":"0")>=0;
}

int is_mhs_ever_docked(void)
{
	int cloned;
	
	if( rdb_get_single("mhs.cloned",rdb_buf,sizeof(rdb_buf))<0 ) {
		syslog(LOG_ERR,"failed to get mhs.cloned - %s",strerror(errno));
		return 0;
	}
	else {
		cloned=atoi(rdb_buf);
	}
	
	syslog(LOG_INFO,"cloned = %d", cloned);
	
	return cloned;
}

int set_operation_mode_simple(int docked,int chargeronly,int mode_chg)
{
	int engaging;
	
	syslog(LOG_INFO,"### applying *simple mode* - docked=%d,chargeronly=%d",docked,chargeronly);
	
	set_mhs_cloned_status(0);
	
	engaging=docked && !chargeronly;
	
	// use br0 alias if not docked
	setup_mhs_alias(!engaging);
	setup_ebtables(engaging);
	
	if(engaging) {
		// enable eth physical link
		control_eth_phy(1);
		// apply MHS configuraiton if docked
		clone_mhs_wifi(operation_simple,0,1);
		// send docking command
		control_mhs_dock();
	}
	else {
		// disable eth physical link
		control_eth_phy(0);
		// apply previous configuraiton if the mode is changed
		clone_mhs_wifi(operation_simple,1,0);
	}
	
	return 0;
}

int set_operation_mode_soho(int docked,int chargeronly,int mode_chg)
{
	int engaging;
	
	syslog(LOG_INFO,"### applying *soho mode* - docked=%d,chargeronly=%d",docked,chargeronly);
	
	set_mhs_cloned_status(0);
	
	// we do not use br0 for soho mode
	setup_mhs_alias(0);
	// clear mac redirect & arp reply
	setup_ebtables(0);
	// enable eth physical link
	control_eth_phy(1);
	
	engaging=docked && !chargeronly;

	if(engaging) {
		// apply MHS configuraiton only if previously not docked
		clone_mhs_wifi(operation_soho,0,1);
		// send docking command
		control_mhs_dock();
	}
	else {
		// apply previous configuraiton if the mode is changed
		clone_mhs_wifi(operation_soho,1,1);
	}
	
	return 0;
}

int set_operation_mode_poweruser(int docked,int chargeronly,int mode_chg)
{
	int engaging;
	
	syslog(LOG_INFO,"### applying *poweruser mode* - docked=%d,chargeronly=%d",docked,chargeronly);
	
	engaging=docked && !chargeronly;
	
	// we do not use br0 for soho mode
	setup_mhs_alias(0);
	// clear mac redirect & arp reply
	setup_ebtables(0);
	// enable eth physical link
	control_eth_phy(1);
	
	// clone wifi configuration if docked and ever cloned
	if(engaging) {
		// apply MHS configuraiton only if previously not docked
		if(!is_mhs_ever_docked()) {
			clone_mhs_wifi(operation_advance,0,1);
			set_mhs_cloned_status(1);
		}
		// send docking command
		control_mhs_dock();
	}
	else {
		if(mode_chg) {
			// apply previous configuraiton if the mode is changed
			clone_mhs_wifi(operation_advance,1,1);
		}
	}
	
	return 0;
}

int set_operation_mode(int docked,int chargeronly,const char* opmode,int mode_chg)
{
	int stat;
	
	syslog(LOG_INFO,"new status - docked=%d,chargeronly=%d,opmode=%s,mode_chg=%d",docked,chargeronly,opmode,mode_chg);
	
	
	if(!strcmp(opmode,"simple")) {
		stat=set_operation_mode_simple(docked,chargeronly,mode_chg);
	}
	else if(!strcmp(opmode,"soho")) {
		stat=set_operation_mode_soho(docked,chargeronly,mode_chg);
	}
	else if(!strcmp(opmode,"poweruser")) {
		stat=set_operation_mode_poweruser(docked,chargeronly,mode_chg);
	}
	else {
		syslog(LOG_ERR,"unknown operation mode detected - %s",opmode);
		stat=-1;
	}
	
	static int pre_docked=0;
	static int first_time=1;
	
	// if docking status changed
	if( first_time || (!pre_docked && docked) || (pre_docked && !docked) ) {
		
		if(docked) {
			syslog(LOG_INFO,"cradle docked");
			
			rdb_set_single("wwan.0.mhs.docked","1");
		}
		else {
			syslog(LOG_INFO,"cradle undocked");
			
			rdb_set_single("wwan.0.mhs.docked","0");
		}
		
		pre_docked=docked;
		first_time=0;
	}
	
	return stat;
}

int main(int argc,char* argv[])
{
	sigset_t mask;
	sigset_t orig_mask;
 
	int rdb_fd;
	int max_fd;
	int stat;
	
	fd_set fdr;
	int chg;
	
	// openlog
	openlog("opmode", LOG_PID | LOG_PERROR, LOG_UPTO(LOG_DEBUG));
	
	syslog(LOG_INFO,"configurating signals");
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGTERM, sig_handler);
	signal(SIGHUP, sig_handler);
	
	// open rdb
	syslog(LOG_INFO,"opening database");
	if(rdb_open_db()<0) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}
	
	create_rdb();
	subscribe_rdb();
	
	// waiting loop
	
	// get rdb handle
	rdb_fd=rdb_get_fd();
	max_fd=rdb_fd+1;
	
	
	syslog(LOG_INFO,"blocking signals");
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
 
	if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
		syslog(LOG_ERR,"failed to block signal - %s",strerror(errno));
		return 1;
	}
	
	// entering signal-free zone now
	force_trigger=1;
	
	stat=1;
	
	while(running) {
		
		//syslog(LOG_INFO,"reading current triggers");
		update_cur_triggers();
		
		// if SIGHUP signal interrupt
		if(force_trigger) {
			syslog(LOG_INFO,"force flag detected");

			chg=1;
			force_trigger=0;
		}
		else {
			//syslog(LOG_INFO,"checking if any changed");
			chg=is_any_changed();
		}
		
		
		if(chg) {
			//syslog(LOG_INFO,"changed");
			
			int docked;
			const char* opmode;
			const char* opmode_prev;
			int chargeronly;
			int mode_chg;
			
			// get docked status
			docked=!strcmp(cur_triggers[2],"ready");
			// get charging only status
			chargeronly=atoi(cur_triggers[1]);
			// get operation mode
			opmode=cur_triggers[0];
			opmode_prev=prev_triggers[0];
			
			mode_chg=*opmode_prev && strcmp(opmode,opmode_prev);
			
			// if not docked, otherwise mgr has to be ready if docked
			syslog(LOG_INFO,"setting operation mode");
			if(set_operation_mode(docked,chargeronly,opmode,mode_chg)>=0)
				update_prev_triggers();
		}
		
		// init fdr
		FD_ZERO(&fdr);
		FD_SET(rdb_fd, &fdr);
		
		struct timeval tv;
		
		tv.tv_sec=2;
		tv.tv_usec=0;
		
		// pselect is buggy and does not exist in many platforms. I use this way
		// there are still race condition graps between those functions calls but the tiimeout short enough to recover
		sigprocmask(SIG_SETMASK, &orig_mask, NULL);
		stat = select(max_fd, &fdr, NULL, NULL, &tv);
		sigprocmask(SIG_SETMASK, &mask, &orig_mask);
		
		if(stat<0) {
			if(errno==EINTR) {
				syslog(LOG_INFO,"punk! interrupt by signal");
			}
			else {
				syslog(LOG_ERR,"unknown error - %s",strerror(errno));
				break;
			}
		}
	}
		
	syslog(LOG_INFO,"closing rdb");
	rdb_close_db();
	
	syslog(LOG_INFO,"closing log");
	closelog();
			
	
	return 0;
}
