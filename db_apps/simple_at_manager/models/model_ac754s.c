/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/times.h> 
#include <errno.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"
#include "../util/rdb_util.h"

#include "../rdb_names.h"
#include "../at/at.h"
#include "../model/model.h"
#include "../util/scheduled.h"

#include "model_default.h"
#include "../util/at_util.h"

#include <openssl/md5.h>

#define CALL_UNTIL_SUCC(func) { \
		static int done=0; \
		if(!done) \
			done=(func)>=0; \
	}
		

// AT command common buffer
static char response[1024];
static int ok;
static char* cmd;
static char* prefix;


extern struct model_t model_default;

int convert_bin_to_asciihex(const char* bin,int len,char* buf,int buflen)
{
	int i;
	int idx;
	
	// check overflow
	if(len*2+1>buflen)
		return -1;
	
	// convert
	idx=0;
	for(i=0;i<len;i++) {
		sprintf(buf+idx,"%02X",(unsigned char)*bin++);
		idx+=2;
	}
	
	buf[idx++]=0;
	
	return idx;
}

int convert_asciihex_to_bin(const char *hex,char* buf,int buflen)
{
	int idx;
	
	int i;
	
	char ch;
	
	int hexlen;
	int nibble;
	
	hexlen=strlen(hex);
	
	if(buflen*2<hexlen)
		return -1;
	
	idx=0;
	
	for(i=0;i<hexlen;i++) {
		
		// clear if even
		if(!(i&0x01))
			buf[idx]=0;
		
		// convert
		if( (ch=tolower(hex[i])) >= 'a' )
			nibble=ch-'a'+0x0a;
		else
			nibble=ch-'0'+0x00;
		
		buf[idx]=(char)((buf[idx]<<4) | (nibble&0x0f));
		
		// inc if odd
		if(i&0x01)
			idx++;
	}
	
	return idx;
}

int generate_rand_buf(int bytes,char* buf,int buflen)
{
	clock_t now;
	int i;
	struct tms tmsbuf;
	
	if(buflen<bytes)
		return -1;
	
	now=times(&tmsbuf);
	
	// set seed
	srand((unsigned int)now);
	
	// collect randoms
	for(i=0;i<bytes;i++) {
		buf[i]=(char)(rand()&0xff);
	}
			
	return 0;
}

int md5sum(const char* data,int len,char* buf,int buflen)
{
	MD5_CTX c;
	
	if(buflen<16)
		return -1;
	
	MD5_Init(&c);
	MD5_Update(&c, data, len);
	MD5_Final(buf, &c);

	return 16;
}

static int sync_ssid_passphr()
{
	char ssid[128];
	char passphr[128];
	
	if(rdb_get_single("wlan.0.ssid",ssid,sizeof(ssid))<0) {
		syslog(LOG_ERR,"failed get get ssid - %s",strerror(errno));
		return -1;
	}
	
	if(!*ssid) {
		syslog(LOG_ERR,"failed to sync - ssid does not exist yet");
		return -1;
	}
	
	if(rdb_get_single("wlan.0.wpa_pre_shared_key",passphr,sizeof(passphr))<0) {
		syslog(LOG_ERR,"failed get get passphrase - %s",strerror(errno));
		return -1;
	}
	
	char atcmd[128];
	
	syslog(LOG_INFO,"sync: current our ssid = %s",ssid);
	
	// set ssid
	snprintf(atcmd,sizeof(atcmd),"AT!HSCRADLEWLANSSID=\"%s\"",ssid);
	
	// get nonce
	if( at_send(atcmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",atcmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",atcmd,response);
		return -1;
	}
	
	// set ssid
	snprintf(atcmd,sizeof(atcmd),"AT!HSCRADLEWLANPASSPHR=\"%s\"",passphr);
	
	// get nonce
	if( at_send(atcmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",atcmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",atcmd,response);
		return -1;
	}
	
	return 0;
}

static int set_mhs_wlan_enable(int en)
{
	if(en) {
		syslog(LOG_INFO,"turning on MHS WLAN");
		
		cmd="AT!HSWLANENABLE=1";
	}
	else {
		syslog(LOG_INFO,"turning off MHS WLAN");
		
		cmd="AT!HSWLANENABLE=0";
	}
	
	if( at_send(cmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	return 0;
}

// static int update_msh_pdp_status(void)
// {
// 	
// /*
// 	AT!SCACT?1
// 	ERROR
// 	
// 	AT!SCACT?1
// 	!SCACT: 1,0
// 
// 	OK
// */
// 	
// 	cmd="AT!SCACT?1";
// 	prefix="!SCACT: ";
// 	
// 	if( at_send(cmd, response, prefix, &ok, sizeof(response))<0 ) {
// 		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
// 		return -1;
// 	}
// 	
// 	if(!ok) {
// 		rdb_set_single(rdb_name(RDB_PDP0STAT, ""), "down");
// 		return 0;
// 	}
// 	
// 	int pid;
// 	int up;
// 	
// 	if( sscanf(response+strlen(prefix),"%d,%d",&pid,&up) != 2 ) {
// 		syslog(LOG_ERR,"incorrect response format - cmd=%s,resp=%s",cmd,response);
// 		return -1;
// 	}
// 	
// 	rdb_set_single(rdb_name(RDB_PDP0STAT, ""), (up==0)?"down":"up");
// 
// 	return 0;
// }


static int mhs_init(void)
{
	
/*
	AT!HSOPENADMIN?
	<nonce>
	OK
	
	AT!HSOPENADMIN=<digest>
	OK
	
*/	
	cmd="AT!HSCRADLEMODE?";
	
	// check to see if password unlocked is already or not
	if( (at_send(cmd, response, "", &ok, sizeof(response))>=0) && ok ) {
		syslog(LOG_INFO,"level 1a already unlocked");
		return 0;
	}
	
	cmd="AT!HSOPENADMIN?";
	// get nonce
	if( at_send(cmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		
		// assume the password unlocked
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	// check validation - nonce
	if( strlen(response)!=16 ) {
		syslog(LOG_ERR,"incorrect nonce - %s",response);
		return -1;
	}
	
	// get digest - MD5
	char* nonce;
	char nonce_passwd[16+16]; // nonce(16 digits) + digest(16 digits)
	char digest[16];
	const char* passwd="F03CD6B4A2120597";
	int len;
	
	// add nonce
	nonce=response;
	syslog(LOG_INFO,"nonce - %s",nonce);
	
	// check nonce length
	len=strlen(nonce);
	if(len!=16) {
		syslog(LOG_ERR,"nonce length is not 16 - len=%d",len);
		return -1;
	}
	
	// check passwd length
	len=strlen(passwd);
	if(len!=16) {
		syslog(LOG_ERR,"passwd length is not 16 - len=%d",len);
		return -1;
	}
	
	// build nonce password
	memcpy(nonce_passwd,nonce,16);
	memcpy(nonce_passwd+16,passwd,16);
			
	// md5sum
	if(md5sum(nonce_passwd,sizeof(nonce_passwd),digest,sizeof(digest))<0) {
		syslog(LOG_ERR,"buffer overflow for digest - bufsize=%d",sizeof(digest));
		return -1;
	}
	
	// convert digest to ascii
	char ascii_digest[16*2+1];
	if(convert_bin_to_asciihex(digest,sizeof(digest),ascii_digest,sizeof(ascii_digest))<0) {
		syslog(LOG_ERR,"buffer overflow for asscii digest - bufsize=%d",sizeof(ascii_digest));
		return -1;
	}
		
	// send level 1a password
	char cmdp1a[64];
	
	snprintf(cmdp1a,sizeof(cmdp1a),"AT!HSOPENADMIN=%s",ascii_digest);
	
	// get nonce
	if( at_send(cmdp1a, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmdp1a);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmdp1a,response);
		return -1;
	}
	
	syslog(LOG_INFO,"level 1a password successfully unlocked");
	
	return 0;
}

static int update_mhs_sim_status(void)
{
	
/*
	AT+CPIN?
	+CME ERROR: SIM not inserted
	
	AT+CPIN?
	ERROR
	
	AT+CPIN?
	+CPIN: READY

	OK
*/	
	
	cmd="AT+CPIN?";
	prefix="";
	
	if(at_send(cmd, response,prefix, &ok, sizeof(response))<0) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	// get result
	char* result;
	prefix=": ";
	if((result=strstr(response,prefix))==0) {
		rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), "SIM not inserted");
		return 0;
	}
	
	// get sim status
	char* sim_stat;
	sim_stat=result+strlen(prefix);
	
	// change string to backward compatibility
	if(!strcmp(sim_stat,"READY"))
		sim_stat="SIM OK";
	
	rdb_set_single(rdb_name(RDB_SIM_STATUS, ""), sim_stat);
	
	return 0;
}

const char* convert_to_macaddr(char* mac)
{
	static char mac_buf[sizeof("00:60:64:5E:12:5E")+1];
	int d=0;
	int s=0;
	int len;
	
	// 84DB2F080CB1
	
	len=strlen(mac);
	while( (d<sizeof(mac_buf)-1) && (s<len) ) {
		if( ((s&0x01)==0) && (s>=2) && (s<=10) )
			mac_buf[d++]=':';
		mac_buf[d++]=mac[s++];
	}
	
	mac_buf[d++]=0;
	
	return mac_buf;
}

const char* convert_to_ipaddr(char* addr)
{
	struct in_addr in;
	
	if( sscanf(addr,"%i",&in.s_addr)!=1 )
		return 0;
	
	in.s_addr=htonl(in.s_addr);
	
	return inet_ntoa(in);
}

static int update_mhs_config_convert(const char* at_cmd_operand,const char* (*convert)(char*))
{
	char at_cmd[128];
	char res_prefix[128];
	char rdb_suffix[128];
	const char* rdb;
	
	// build AT cmd and prefix
	snprintf(at_cmd,sizeof(at_cmd),"AT!%s?",at_cmd_operand);
	snprintf(res_prefix,sizeof(res_prefix),"!%s:",at_cmd_operand);
	
	// get rdb
	int i=0;
	while( ( (rdb_suffix[i]=tolower(at_cmd_operand[i++]))!=0 ) && ( i<sizeof(rdb_suffix) ) );
	rdb=rdb_name("mhs", rdb_suffix);
	
	if(at_send(at_cmd, response,"", &ok, sizeof(response))<0) {
		syslog(LOG_ERR,"failed to send cmd - %s",at_cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",at_cmd,response);
		return -1;
	}
	
	char* ptr;
	
	// get prefix
	ptr=strstr(response, res_prefix);
	if(!ptr) {
		syslog(LOG_ERR,"incorrect response format / missing PREFIX - cmd=%s,resp=%s,prefix=%s",cmd,response,res_prefix);
		return -1;
	}
	
	char* eol;
	prefix="\n";
	eol=strstr(ptr,prefix);
	if(!eol) {
		syslog(LOG_ERR,"incorrect response format / missing EOL - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	// convert
	const char* val=0;
	if(convert)
		val=convert(eol+strlen(prefix));
	
	// rdb set
	if(val) {
		rdb_set_single(rdb, val);
		syslog(LOG_INFO,"mhs setting / '%s' - '%s'",rdb,val);
	}
	else {
		rdb_set_single(rdb, eol+strlen(prefix));
		syslog(LOG_INFO,"mhs setting / '%s' - '%s'",rdb,eol+strlen(prefix));
	}
	
	
	return 0;
}

static int update_mhs_config(const char* at_cmd_operand)
{
	return update_mhs_config_convert(at_cmd_operand,0);
}

static int update_mhs_configs(void)
{
	int stat;
	
	stat=0;
	
	if(update_mhs_config("HSWLANPHYMODE")<0)
		stat=-1;
	
	if(update_mhs_config("HSWLANSSID")<0)
		stat=-1;
	
	if(update_mhs_config("HSWLANHIDDENSSID")<0)
		stat=-1;
	
	if(update_mhs_config("HSWLANCHAN")<0)
		stat=-1;
	
	if(update_mhs_config_convert("HSWLANIP",convert_to_ipaddr)<0)
		stat=-1;
	
	if(update_mhs_config_convert("HSWLANMAC",convert_to_macaddr)<0) {
		syslog(LOG_ERR,"AT!HSWLANMAC does not seem to work - check your MHS version");
		stat=-1;
	}
	
	if(update_mhs_config_convert("HSRTRNETMASK",convert_to_ipaddr)<0)
		stat=-1;
	
	if(update_mhs_config("HSWLANENCRIP")<0)
		stat=-1;
	
	if(update_mhs_config("HSWLANPASSPHR")<0)
		stat=-1;
	
	//update_mhs_config("HSWLANPROTECT");
	// (syslog(LOG_ERR,"TODO: skipping - HSWLANPROTECT is not supported yet"),1) );
	
	if(update_mhs_config("HSRTRDHCPENA")<0)
		stat=-1;
	
	if(update_mhs_config_convert("HSRTRIPLOW",convert_to_ipaddr)<0)
		stat=-1;
	
	if(update_mhs_config_convert("HSRTRIPHI",convert_to_ipaddr)<0)
		stat=-1;
	
	if(update_mhs_config_convert("HSRTRPRIDNS",convert_to_ipaddr)<0)
		stat=-1;
	
	if(update_mhs_config_convert("HSRTRSECDNS",convert_to_ipaddr)<0)
		stat=-1;
	
	if(update_mhs_config("HSCUSTLASTWIFICHAN")<0)
		stat=-1;
	
	return stat;
}

static int update_mhs_imei(void)
{
	
/*
	AT+CIMI
	ERROR
	
	AT+CIMI
	505013434043185

	OK
*/	
	
	cmd="AT+CIMI";
	prefix="";
	
	if(at_send(cmd, response,prefix, &ok, sizeof(response))<0) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	// check validation
	if(strlen(response)!=15) {
		syslog(LOG_ERR,"incorrect response format - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	rdb_set_single(rdb_name(RDB_IMSI".msin", ""), response);
	
	return 0;
}

static int update_mhs_ccid(void)
{
	
/*
	AT!ICCID?
	ERROR
	
	AT!ICCID?
	!ICCID: 89610154253220000085

	OK
*/
	
	cmd="AT!ICCID?";
	prefix="!ICCID: ";
	
	if(at_send(cmd, response,prefix, &ok, sizeof(response))<0) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	rdb_set_single(rdb_name(RDB_NETWORK_STATUS".simICCID", ""), response+strlen(prefix));
	
	return 0;
}

static int do_cradle_procedure_1(void)
{
	static int done=0;
	
	if(done)
		return 0;
	
	// send event
	cmd="AT!HSEVENT=0,7";
	
	// get nonce
	if( at_send(cmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	done=1;
	
	return 0;
}
static int do_cradle_procedure_2(void)
{
	
	static int done=0;
	
	if(done)
		return 0;
	
	// send cradle mode command
	cmd="AT!HSCRADLEMODE=1";
	
	// get nonce
	if( at_send(cmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}

	// send event
	cmd="AT!HSEVENT=0,7";
	
	// get nonce
	if( at_send(cmd, response, "", &ok, sizeof(response))<0 ) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
/*	
	// sync
	if(sync_ssid_passphr()<0) {
		syslog(LOG_ERR,"failed to sync ssid and passphrase");
		return -1;
	}
*/	
	
	done=1;
	
	return 0;
}

static int update_mhs_boot_version(void)
{
/*
	AT!BCINF
	OSBL
	Addr: 00BFF200
	Ver:  SWI9200H_01.00.03.02BT R2933 CARMD-EN-10526 2011/08/22 12:03:03
	Date: 08/22/11
	Size: 0008D350

	AMSS
	Addr: 00BFF400
	Ver:  SWI9200H_01.00.03.02AP R2933 CARMD-EN-10526 2011/08/22 12:12:05
	Date: 08/22/11
	Size: 00242E70
	
	.
	.
	.

	OK
*/		
	cmd="AT!BCINF";
	
	if(at_send(cmd, response,"", &ok, sizeof(response))<0) {
		syslog(LOG_ERR,"failed to send cmd - %s",cmd);
		return -1;
	}
	
	if(!ok) {
		syslog(LOG_ERR,"error returned - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	char* ptr;
	char* eol;
	
	// get boot loader section
	ptr=strstr(response, "OSBL");
	if(!ptr) {
		syslog(LOG_ERR,"incorrect response format / missing OSBL - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	// get version
	prefix="Ver:  ";
	ptr=strstr(response, prefix);
	if(!ptr) {
		syslog(LOG_ERR,"incorrect response format / missing Ver - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	
	// get end of line
	eol=strstr(ptr,"\n");
	if(!eol) {
		syslog(LOG_ERR,"incorrect response format / missing EOL - cmd=%s,resp=%s",cmd,response);
		return -1;
	}
	*eol=0;
	
	rdb_set_single(rdb_name(RDB_MODULE_BOOT_VERSION, ""), ptr+strlen(prefix));
	
	return 0;
}



int mhs_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status,const struct model_status_info_t* err_status)
{
	int i;
	
	static int succ_cradle=-1;
	static int succ_mhs_config=-1;
	
	// cardle startup procedure
	i=0;
	while( (succ_cradle<0) || (succ_mhs_config<0)) {
		
		// cradle procedure
		CALL_UNTIL_SUCC(succ_cradle=do_cradle_procedure_1());
		// mhs config
		CALL_UNTIL_SUCC(succ_mhs_config=update_mhs_configs());
		
		if( (succ_cradle<0) || (succ_mhs_config<0)) {
			syslog(LOG_ERR,"failed to do mhs dock procedure");
		}
		else {
			syslog(LOG_ERR,"mhs dock procedure successfully finished");
			break;
		}
		
		i++;
		
		// bypass if we tr
		if(i<3) {
			syslog(LOG_ERR,"trying mhs dock procedure again #%d",i);
		}
		else {
			syslog(LOG_ERR,"completely failed to do mhs dock procedure");
			break;
		}
	}
	
	// if sim card ready
	if(new_status->status[model_status_sim_ready]) {
		// update CCID
		CALL_UNTIL_SUCC(update_mhs_ccid());
		// update IMEI
		CALL_UNTIL_SUCC(update_mhs_imei());
	}
	
	// update bootloader version
	CALL_UNTIL_SUCC(update_mhs_boot_version());
		
	// update SIM CARD
	update_mhs_sim_status();
	
	// update network
	update_network_name();
	// update service
	update_service_type();
	// update signal strength
	update_signal_strength();
	// update pdp status - replaced by HSEVENT
	//update_msh_pdp_status();
	
	return 0;
}

static int mhs_detect(const char* manufacture, const char* model_name)
{
	const char* model_names[]={
		"AC754S",
  		"AC760S",
	};
	

	// search Sierra in manufacture string
	if(!strstr(manufacture,"Sierra"))
		return 0;

	// compare model name
	int i;
	for (i=0;i<sizeof(model_names)/sizeof(const char*);i++)
	{
		if(!strcmp(model_names[i],model_name))
			return 1;
	}
	
	if(!strncmp(model_name,"AC7",3)) {
		syslog(LOG_INFO,"unknown modem but starting with AC7 - the router assumes AC7 series");
		return 1;
	}
	
	
	return 0;
}

static int mhs_handle_command(const struct name_value_t* args)
{
	if (!args || !args[0].value) {
		return -1;
	}
	if (strcmp(args[0].value, "enablewlan") == 0) {
		set_mhs_wlan_enable(1);
	}
	else if (strcmp(args[0].value, "disablewlan") == 0) {
		set_mhs_wlan_enable(0);
	}
	else if (strcmp(args[0].value, "syncssid") == 0) {
		sync_ssid_passphr();
	}
	else if (strcmp(args[0].value, "dock") == 0) {
		do_cradle_procedure_2();
		sync_ssid_passphr();
	}
	else {
		syslog(LOG_ERR,"unknown mhs command - %s",args[0].value);
	}

	return 0;
}

static int mhs_noti_event(const char* event)
{
	int change_mask;
	int state_mask;
	int count;
	
	int bit;
	int mask;
	int value;
	
	const char* bit_rdb_variables[]={
		"mhs.wps",
		"mhs.chargingonlymode",
  		"mhs.wwan",
		0,
	};
	
	
	syslog(LOG_INFO,"hsevent - %s",event);
	
/*
	HSEVENT example
	
	!HSEVENT: 0,00000007,0000000
	
	!HSEVENT: <reserved>,<change mask>,<state mask>
*/
	
	// read change mask and state mask
	count=sscanf(event,"!HSEVENT: 0,%x,%x",&change_mask,&state_mask);
	
	// check validation
	if(count!=2) {
		syslog(LOG_ERR,"incorrect format in HSEVENT - %s",event);
		return -1;
	}
	
	syslog(LOG_INFO,"change_mask=0x%08x,state_mask=0x%08x",change_mask,state_mask);
	
	// convert bit to rdb variable
	bit=0;
	while(bit_rdb_variables[bit] && (bit<sizeof(mask)*8)) {
		
		mask=1<<bit;
		
		// bypass if not changed
		if(change_mask && mask) {
			// get value
			if(state_mask&mask)
				value=1;
			else
				value=0;
			
			syslog(LOG_INFO,"bit changed - %s=%d",bit_rdb_variables[bit],value);
			
			// set rdb value
			rdb_set_single_int(rdb_name(bit_rdb_variables[bit],""),value);
			
			// if wwan
			if(bit==2)
				rdb_set_single(rdb_name(RDB_PDP0STAT, ""), value?"up":"down");
		}
		
		bit++;
	}
	
	return 0;
}

struct command_t mhs_commands[] =
{
	{ .name = "mhs.command",	.action = mhs_handle_command },
 	{0,}
};

const struct notification_t mhs_notifications[] =
{
	{ .name = "!HSEVENT:", .action = mhs_noti_event },
	{0, } // zero-terminator
};

struct model_t model_mhs = {
	.name = "sierra MHS",
	.init = mhs_init,
	.detect = mhs_detect,

	.get_status = model_default_get_status,
	.set_status = mhs_set_status,

	.commands = mhs_commands,
	.notifications = mhs_notifications
};

////////////////////////////////////////////////////////////////////////////////
