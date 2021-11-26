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

#include "switchd.h"

#include "logger.h"
#include "daemon.h"
#include "rdb_ops.h"
#include "m88e6060.h"
#include "physmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <linux/mii.h>
#include <dirent.h>

typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#define PRCHAR(x) (((x)>=0x20&&(x)<=0x7e)?(x):'.')
#define MAX_RDB_VAR_LEN	256
#define MAX_RDB_VAL_LEN 256

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
}

static void pabort(const char *s)
{
	perror(s);
	abort();
}

/* Version Information */
#define VER_MJ		0
#define VER_MN		2
#define VER_BLD	2

/* Globals */

/*
 * Global variables shared between all functions.
 */
struct _glb {
	int		run;
	int		verbosity;
	
	/*
	 * RDB Session to ensure thread safe access to RDB functions.
	 */
	struct rdb_session* rdb_session;
}glb;

#define DAEMON_NAME "switchd"

/* The user under which to run */
#define RUN_AS_USER "system"

/* Maximum length of arguments passed to -q and -s */
#define ARGMAX 12

#if (defined V_ETH_PORT_8plllllllw_l)
#define NUM_ETH_PORTS 8
#elif (defined V_ETH_PORT_2plw_l)
#define NUM_ETH_PORTS 2
#else
#define NUM_ETH_PORTS 1
#endif

#define MDIO_RW_DELAY	100		/* micro seconds */

struct MAC_PAIR{
	int port;
	char mac[20];
	int mac12; //1st and 2nd octets of MAC
	int mac34; //3rd and 4th octets of MAC
	int mac56; //5th and 6th octets of MAC
}portMac[NUM_ETH_PORTS];


const char shortopts[] = "pdq:s:vV?a:q:s:";
static char rdb_buf[MAX_RDB_VAL_LEN];

void usage (char **argv)
{
	fprintf(stderr, "\nUsage: %s [-d] [-V] \n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-d Don't detach from controlling terminal (don't daemonise)\n");

	fprintf(stderr, "\t-a [regnum] : Display the given register for all addresses\n");
	fprintf(stderr, "\t-q [address].[regnum]: query the given address/register and print"
			" the result.\n");
	fprintf(stderr, "\t-s [address].[regnum].[value]: set the given address/register and"
			" print the result.\n");

	fprintf(stderr, "\t-v Increase verbosity\n");
	fprintf(stderr, "\t-V Display version information\n");
	fprintf(stderr, "\n");
}

void shutdown_d (int n)
{
	log_INFO("exiting");
	rdb_close(&glb.rdb_session);
	closelog();
	exit (n);
}

static void toggle_debug_level(void)
{
	static int toggle_mode = 0;
	if (toggle_mode) {
		glb.verbosity = toggle_mode = 0;
	} else {
		glb.verbosity = toggle_mode = 1;
	}
}

static void sig_handler(int signum)
{
	log_INFO("Caught Sig %d\n", signum);

	switch(signum)
	{
	case SIGHUP:
		//rdb_import_config (config_file_name, TRUE);
		break;

	case SIGTERM:
		glb.run = 0;
		break;


	case SIGUSR2:
		toggle_debug_level();
	case SIGCHLD:
		wait(NULL);		// Prevent creation of Zombies
	}
}

/*
 * Read a string (into a global buffer) and return a pointer to it.
 * Do not store the pointer for later use as the contents will be
 * overwritten in the next call.
 */
const char* rdb_get_str(const char* var)
{
	if (rdb_get_string(glb.rdb_session, var, rdb_buf, sizeof(rdb_buf))<0) {
		log_ERR ("failed to read %s - %s",var,strerror(errno));
		return NULL;
	}

	return rdb_buf;
}

int mdio_read(int skfd, struct ifreq *ifr, int phy_id, int reg_num)
{
	struct mii_ioctl_data *mii_data = (struct mii_ioctl_data *)(void*)
										(&ifr->ifr_data);
	int cmd;
	char *op;

#if (defined V_ETH_PORT_8plllllllw_l)
	cmd = SIOCGMIIREG;
	op = "SIOCGMIIREG";
#else
	cmd = SIOCGMIIPHY;
	op = "SIOCGMIIPHY";
#endif
	mii_data->phy_id = phy_id;
	mii_data->reg_num = reg_num;
	if (ioctl(skfd, cmd, ifr) < 0)
	{
		fprintf(stderr, "%s on %s failed: %s\n", op, ifr->ifr_name,
				strerror(errno));

		return -1;
	}
	return mii_data->val_out;
}

int mdio_write(int skfd, struct ifreq *ifr, int phy_id, int reg_num, int val)
{
	struct mii_ioctl_data *mii_data = (struct mii_ioctl_data *)(void*)
										(&ifr->ifr_data);

	mii_data->phy_id = phy_id;
	mii_data->reg_num = reg_num;
	mii_data->val_in = val;

	if (ioctl(skfd, SIOCSMIIREG, ifr) < 0)
	{
		fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr->ifr_name,
				strerror(errno));
		return -1;
	}
	return mii_data->val_out;
}

#if (defined USE_SYSFS)
/*
 * Search for num_input_params files in search_path with the names
 * in input_params.
 * 
 * Store the contents of each in the corresponding positions in
 * output_values.
 */
int get_string_params_from_sysfs(const char *search_path, char *input_params[] , int num_input_params, char *output_values[]) {
	char	filename[100];
	FILE	*pf = NULL;
	char	buffer[100];
	int	bytes_read, i;
	DIR*	dirFile;

	for(i = 0;i < num_input_params; i++) {
	  memset(filename, '\0', 100);
	  memset(buffer, '\0', 100);

	  /* Open the directory passed in search_path */
	  dirFile = opendir( search_path );
	  if ( dirFile )
	  {
	      struct dirent* hFile;
	      errno = 0;
	      while (( hFile = readdir( dirFile )) != NULL )
	      {
	          if ( !strcmp( hFile->d_name, "."  )) continue;
	          if ( !strcmp( hFile->d_name, ".." )) continue;

	          /* in linux hidden files all start with '.' */
	          if ( hFile->d_name[0] == '.' ) continue;

	          /* dirFile.name is the name of the file. Do whatever string comparison
	          you want here. */
	          if ( strstr( hFile->d_name, input_params[i] )) {
	            // fprintf( stderr, "found an input_params[i] file: %s\n", hFile->d_name );
	            sprintf(filename, "%s/%s", search_path, hFile->d_name);
	          }
	      }
	      closedir( dirFile );
	  }
	  else {
	      fprintf(stderr, "Could not open directory %s\n", search_path);
	      return -1;
	  }

	  /* Open the constructed filename for reading */
	  if(*filename) {
	    pf = fopen(filename, "r");
	    if(!pf){
	      fprintf(stderr, "Could not open file for output.\n");
	      return -1;
	    }

	    /* Read the respective file */
	    bytes_read = fread(buffer, 1, sizeof(buffer), pf);
	    if(bytes_read == 0 || bytes_read == sizeof(buffer)) {
	      // fprintf(stderr, "Read failed or buffer isn't big enough.\n");
	      return -1;
	    }
	    buffer[bytes_read] = '\0';

	    /* copy value from filesystem to output array */
	    strncpy(output_values[i], buffer, bytes_read);
	    if(fclose(pf)) {
	      fprintf(stderr, "Could not close file.\n");
	      return -1;
	    }
	  }
	  else {
	    fprintf(stderr, "Could not find file %s in search_path %s\n", input_params[i], search_path);
	    return -1;
	  }
	  /*log_INFO ("param: %s, value=%s\n", input_params[i], output_values[i]);*/
	}
	return 0;
}
#endif

#define NUM_INPUT_PARAMS 3

#if (defined USE_SYSFS)
int update_sysfs_port_status(const char* search_path, char* stat_str)
{
	char *output_values[NUM_INPUT_PARAMS];
	int	nFlags=0;
	int i,j;

	memset(output_values, 0, sizeof(output_values));

	sprintf (stat_str, "drl-");

	#define MAX_SIZE 100
	char	*input_params[NUM_INPUT_PARAMS] = {
	  "carrier",
	  "speed",
	  "duplex"
	};

	for(i = 0;i < NUM_INPUT_PARAMS; i++) {
	  output_values[i] = (char *)malloc(MAX_SIZE);
	  if(!output_values[i]) {
	    fprintf(stderr, "malloc failed.\n");

		/* free those allocated before the allocation failure occurred. */
		for(j = 0; j < i; j++) {
			free(output_values[j]);
		}

	    return 1;
	  }
	  else
	    memset(output_values[i], '\0', MAX_SIZE);
	}

	if(get_string_params_from_sysfs(search_path, input_params, NUM_INPUT_PARAMS, output_values) < 0) {
		// fprintf(stderr, "Could not obtain values from sysfs.\n");

		for(i = 0;i < NUM_INPUT_PARAMS; i++) {
		  free(output_values[i]);
		}

	  return 2;
	}

	sprintf (stat_str, "%s%s%s%s",
	  (strncmp(output_values[0], "1", 1) == 0) ? "u" : "d",
	  "r",
	  (atoi(output_values[1]) >= 1000) ? "g" : (atoi(output_values[1]) >= 100) ? "h" : "l",
	  (strncmp(output_values[2], "full", 4) == 0) ? "f" : "-"
	);

	for(i = 0;i < NUM_INPUT_PARAMS; i++) {
	    if(output_values[i])
	      free(output_values[i]);
	}

	return 0;
}

#ifdef V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y
void update_vlan_port_status()
{
	int	nFlags=0;
	int result = 0;
	int vlanindex = 0;
	while (result == 0)
	{
		int vlanID;
		char db_name[32];
		sprintf(db_name, "vlan.%d.vlanid", vlanindex);
		result = rdb_get_single_int(db_name, &vlanID);

		if (result == 0)
		{
			sprintf(db_name, "vlan.%d.dev", vlanindex);
			char* device = rdb_get_str(db_name);

			char stat_str[8];
			char search_path[100];
			memset(search_path, '\0', 100);
			sprintf(search_path, "/sys/class/net/%s.%d", device, vlanID);

			update_sysfs_port_status(search_path, stat_str);

			sprintf(db_name, "vlan.%d.status", vlanindex);
			rdb_update_string(glb.rdb_session,db_name, stat_str,nFlags,DEFAULT_PERM);

			vlanindex++;
		}
	}
}
#endif
#endif

#ifdef  V_IOBOARD_kudu
static void set_ifreq_to_ifname(struct ifreq *ifreq, const char *ethname)
{
	memset(ifreq, 0, sizeof(struct ifreq));
	strncpy(ifreq->ifr_name, ethname, IFNAMSIZ);
}

// This function is extracted from phytest.
int mdio_read_atheros(int fd, int reg, const char*eth)
{
	struct ifreq ifr;
	struct mii_ioctl_data* mii;
	unsigned reg_val;

	mii = (struct mii_ioctl_data*)(&ifr.ifr_data);
	/* change reg_addr to 16-bit word address, 32-bit aligned */
	reg= (reg& 0xfffffffc) >> 1;

	set_ifreq_to_ifname(&ifr, eth);

	if ( ioctl(fd,  SIOCGMIIPHY, &ifr ) < 0) {
		perror("SIOCGMIIPHY ");
		return -1;
	}
	/* configure register high address */
	mii->phy_id=0x18;
	mii->reg_num =0;
	mii->val_in =  (__u16) ((reg>> 8) & 0x3ff);  /* bit16-8 of reg address */
	if (ioctl(fd,  SIOCSMIIREG, &ifr ) < 0) {
		return -1;
	}
	/* read register in lower address */
	mii->phy_id = 0x10 | ((reg>> 5) & 0x7); /* bit7-5 of reg address */
	mii->reg_num = (__u8) (reg& 0x1f);   /* bit4-0 of reg address */
	if (ioctl(fd,  SIOCGMIIREG, &ifr ) < 0) {
		return -1;
	}
	reg_val = mii->val_out;
	/* read register in higher address */
	reg++;
	mii->phy_id = 0x10 | ((reg>> 5) & 0x7); /* bit7-5 of reg address */
	mii->reg_num = (__u8) (reg& 0x1f);   /* bit4-0 of reg address */
	if (ioctl(fd,  SIOCGMIIREG, &ifr ) < 0) {
		return -1;
	}
	if (ioctl(fd,  SIOCGMIIREG, &ifr ) < 0) {
		return -1;
	}
	reg_val |= (mii->val_out << 16);

//	log_ERR("SIOCGMIIREG %s: phy_id=%d, reg=0x%x, val_out=0x%x \n", eth, mii->phy_id,  reg-1, reg_val);
	return (__u32)reg_val;
}
#endif

void update_port_status(int skfd, struct ifreq *ifr, int port)
{
	#ifndef USE_SYSFS
	int	status;
	#endif
	int i;
	int	nFlags=0;
	char	stat_str[8];
	char	db_name[32];
	static int err_cnt = 0;
#if (defined USE_SYSFS)
	for(i = 0;i < V_ETH_PORT; i++) {
		char search_path[100];
	    memset(search_path, '\0', 100);
	    sprintf(search_path, "/sys/class/net/eth%d", i);
		update_sysfs_port_status(search_path, stat_str);
	}
#elif (defined V_ETH_PORT_1pl)
	/* read status register */
	status = mdio_read (skfd, ifr, port, 0x01);

	#define PSR_100_T4				0x8000
	#define PSR_100_TX_FDPX				0x4000
	#define PSR_100_TX_HDPX				0x2000
	#define PSR_10_T_FDPX				0x1000
	#define PSR_10_T_HDPX				0x0800
	#define PSR_PREAMBLE_SUP			0x0040
	#define PSR_AUTO_NEGO_COMPLT		0x0020
	#define PSR_REMOTE_FAULT			0x0010
	#define PSR_AUTO_NEGO_AVILITY		0x0008
	#define PSR_LINK_STATUS				0x0004
	#define PSR_JABBER_DETECT			0x0002
	#define PSR_EXTENED_CAPA			0x0001

	#define PSR_10_100_HD_FD			0x7800
	#define PSR_100_HD_FD				0x6000
	#define PSR_10_HD_FD				0x1800

	sprintf (stat_str, "%s%s%s%s",
	    (status & PSR_LINK_STATUS) ? "u" : "d",
	    "r",
	    (status & PSR_100_T4 || status & PSR_100_TX_FDPX || status & PSR_100_TX_HDPX) ? "h" : "l",
	    (status & PSR_100_TX_FDPX || status & PSR_10_T_FDPX) ? "f" : "-"
	);
#elif (defined V_ETH_PORT_2plw_l)
#if (defined V_IOBOARD_kudu)
	#define PORT0_STATUS_ADDR_OFFSET		0x0080
	#define PORT1_STATUS_ADDR_OFFSET		0x0084

	#define LINK_STATUS_MASK			0x0100
	#define DUPLEX_STATUS_MASK			0x0040
	#define SPEED_STATUS_MASK			0x0003

	#define SPEED_10M				0x0000
	#define SPEED_100M				0x0001
	#define SPEED_1000M				0x0002

	if (port == 0){
		status=mdio_read_atheros(skfd, PORT0_STATUS_ADDR_OFFSET, "dummy0");
	}
	else {
		status=mdio_read_atheros(skfd, PORT1_STATUS_ADDR_OFFSET, "dummy0");
	}

	sprintf (stat_str, "%s%s%s%s",
			(status & LINK_STATUS_MASK) ? "u" : "d",
			"r",
			((status & SPEED_STATUS_MASK) == SPEED_1000M) ? "g" : ((status & SPEED_STATUS_MASK) == SPEED_100M) ? "h" : "l",
			(status & DUPLEX_STATUS_MASK) ? "f" : "-"
		);
#else
	status = mdio_read (skfd, ifr, port, 0x01);

	#define RGMII2_FULLDUPLEX			0x0080
	#define RGMII2_1000M_SPEED			0x0040
	#define RGMII2_100M_SPEED			0x0020
	#define RGMII2_LINK_STATUS			0x0010
	#define RGMII1_FULLDUPLEX			0x0008
	#define RGMII1_1000M_SPEED			0x0004
	#define RGMII1_100M_SPEED			0x0002
	#define RGMII1_LINK_STATUS			0x0001

	if (port == 0) {
		sprintf (stat_str, "%s%s%s%s",
			(status & RGMII1_LINK_STATUS) ? "u" : "d",
			"r",
			(status & RGMII1_1000M_SPEED) ? "g" :
			(status & RGMII1_100M_SPEED) ? "h" : "l",
			(status & RGMII1_FULLDUPLEX) ? "f" : "-"
			);
	} else {
		sprintf (stat_str, "%s%s%s%s",
			(status & RGMII2_LINK_STATUS) ? "u" : "d",
			"r",
			(status & RGMII2_1000M_SPEED) ? "g" :
			(status & RGMII2_100M_SPEED) ? "h" : "l",
			(status & RGMII2_FULLDUPLEX) ? "f" : "-"
			);
	}
#endif
#elif (defined V_ETH_PORT_8plllllllw_l)
	status = mdio_read (skfd, ifr, 0x10+port, 0x00);

	#define MY_PHY    0x1000
	#define MY_LINK   0x0800
	#define MY_DUPLEX 0x0400
	#define MY_SPEED  0x0100

	sprintf (stat_str, "%s%s%s%s",
	  (status & MY_LINK) ? "u" : "d",
	  (status & MY_PHY) ? "r" : "-",
	  (status & MY_SPEED) ? "h" : "l",
	  (status & MY_DUPLEX) ? "f" : "-"
	);
#else
	status = mdio_read (skfd, ifr, 0x08+port, 0x00);

	sprintf (stat_str, "%s%s%s%s",
	  (status & PSR_LINK) ? "u" : "d",
	  (status & PSR_RESOLVED) ? "r" : "-",
	  (status & PSR_SPD100) ? "h" : "l",
	  (status & PSR_DUPLEX) ? "f" : "-"
	);
#endif

	/* fprintf(stderr, "stat_str: %s\n", stat_str); */
	sprintf (db_name, "hw.switch.port.%d.status", port);

	if (rdb_update_string(glb.rdb_session,db_name, stat_str,nFlags,DEFAULT_PERM))
	{
	    if (err_cnt++ == 0)
	      log_ERR("Unable to update database: %s", strerror(errno));
	    if (err_cnt >= (4 * 5 * 60))	// Only log 1 error every minute
	      err_cnt = 0;
	}

	/* log_INFO ("Port %d = %04X %s", port, status, stat_str); */

}

#if (defined V_ETH_PORT_8plllllllw_l)
/*
========================================================================

Routine Description:
	Based on portVec read, count the port number that the bit in portVec has been set.

Arguments:
portmap		data read back from DATA register, which has format of
			15:TRUNK 14:4 PortVec 3:0 SPID/Entry State

Return Value:
	portnum 		port number to indicate which port the portVec has been set

Note:

========================================================================
*/
int port_num_convert(int portmap)
{
	int portnum = 0;
	portmap = (portmap>>4);
	// 15:TRUNK 14:4 PortVec 3:0 SPID/Entry State
	if(portmap != 0)
		portnum ++;
	while((portmap>>=1) !=0 )
	{
		portnum ++;
		if(portnum > NUM_ETH_PORTS)
		{
			portnum = 0;
			break;
		}
	}
	return portnum;
}

/*
========================================================================

Routine Description:
	Reading back MAC/port pair and update respective rdb variables

Arguments:
skfd
ifr

Return Value:

Note:

========================================================================
*/
void update_port_mac_map(int skfd, struct ifreq *ifr)
{
	int status;
	int data;
	int mac12;
	int mac34;
	int mac56;
	int portnum=0;
	int	nFlags=0;
	static int err_cnt = 0;
	int i=0, j=0;
	int portmap =0;
	int dhcp_restart=0;
	char portmapstr[12];
	char db_name[32];
	char es[NUM_ETH_PORTS] = {0,};	/* entry state */
	char new_es = 0;
	int found_valid_mac[NUM_ETH_PORTS] = {0,};
	int dhcp_restart_flag[NUM_ETH_PORTS] = {0,};
	static int plugin_cnt[NUM_ETH_PORTS] = {0,};
	char db_mac[32] = {0,};
	//static int not_first[NUM_ETH_PORTS] = {0, };

#define M88E6095_REG_GLOBAL         	0x1b
#define GLOBAL_CONTROL_OFFSET         	10
#define GLOBAL_OPER_OFFSET         		11
#define GLOBAL_DATA_OFFSET         		12
#define GLOBAL_MAC12_OFFSET         	13
#define GLOBAL_MAC34_OFFSET         	14
#define GLOBAL_MAC56_OFFSET         	15
#define MY_LINK   0x0800
	// Disable all those ports with no cable pluged in
	for (i = 0; i < NUM_ETH_PORTS; i++) {
		status = mdio_read (skfd, ifr, 0x10+i, 0x00);
		usleep(MDIO_RW_DELAY);
		if(!(status & MY_LINK)){
			// Disable this port to forward traffic, only allow it to MAC learning
			status = mdio_write (skfd, ifr, 0x10+i, 4, 0x0002);
			usleep(MDIO_RW_DELAY);
		}
	}
	// find lowest mac address first by set all ATU addresses to 1
	status = mdio_write (skfd, ifr, 0x1B, GLOBAL_MAC12_OFFSET, 0xffff);
	usleep(MDIO_RW_DELAY);
	status = mdio_write (skfd, ifr, 0x1B, GLOBAL_MAC34_OFFSET, 0xffff);
	usleep(MDIO_RW_DELAY);
	status = mdio_write (skfd, ifr, 0x1B, GLOBAL_MAC56_OFFSET, 0xffff);
	usleep(MDIO_RW_DELAY);

	log_INFO ("--------------------------------------------------------------------------------");
	while(1)
	{
		// Writing 0xC000 to operation register to read back next MAC and portVec
		status = mdio_write (skfd, ifr, 0x1B, GLOBAL_OPER_OFFSET, 0xC000);
		usleep(MDIO_RW_DELAY);
		data = mdio_read (skfd, ifr, 0x1B, GLOBAL_DATA_OFFSET);
		usleep(MDIO_RW_DELAY);
		mac12 = mdio_read (skfd, ifr, 0x1B, GLOBAL_MAC12_OFFSET);
		usleep(MDIO_RW_DELAY);
		mac34 = mdio_read (skfd, ifr, 0x1B, GLOBAL_MAC34_OFFSET);
		usleep(MDIO_RW_DELAY);
		mac56 = mdio_read (skfd, ifr, 0x1B, GLOBAL_MAC56_OFFSET);
		usleep(MDIO_RW_DELAY);

		log_INFO ("ATU data 0x%04x, mac %04x%04x%04x", data, mac12, mac34, mac56);
		// no more higher or highest mac address when all ATU addresses are 1
		if (mac12 == 0xffff && mac34 == 0xffff && mac56 == 0xffff) {
			if ((data & 0x000f) == 0) {
				log_INFO ("No higher mac address was found");
			} else {
				log_INFO ("Highest mac address was found");
			}
			break;
		}

		// Check if portVec is zero or not
		if((data & 0x7ff0) != 0)
		{
			portnum=port_num_convert(data);
			// portnum = 0 means local MAC
			if(portnum == 0)
				continue;

			/* check entry state field
			 * ES : bit 3:0
			 * 		0 : invalid, empty or purged entry
			 *		1~7 : Aging, 7 is newest
			 */
			new_es = data & 0x000f;
			if (new_es == 0) {
				portMac[portnum-1].mac12 =0;
				portMac[portnum-1].mac34 =0;
				portMac[portnum-1].mac56 =0;
				portMac[portnum-1].mac[0] = 0;
				// if invalid, empty or purged entry, then skip
				log_INFO("clear mac[%d] for es == 0", portnum-1);
				continue;
			}
#if (0)
			else if (new_es != 7) {
				// reduced port checking time to 100 ms, so try previous logic
				log_INFO("es[%d]:%d, continue", portnum-1, new_es);
				continue;
				// When LAN cable is pluged out for short time and pluged in, es changes from 7 to 6 and
				// changes to 7 again but it is not detected as valid plug-in event so need to check here this
				// es status change.
				// if shortly pluged out --> in, then process same as link down
				log_INFO("***************************************************************");
				log_INFO("es[%d]:%d, detected short time cable missing, clear mac[%d]", portnum-1, new_es, portnum-1);
				log_INFO("***************************************************************");
				goto cable_missing;
			}
#endif

			// if new es value is smaller than last value, ignore this
			if (new_es <= es[portnum-1]) {
				continue;
			} else {
				es[portnum-1] = new_es;
			}

			// count the number of valid mac address for same port
			found_valid_mac[portnum -1]++;
			log_INFO("found_valid_mac[%d] = %d", portnum-1, found_valid_mac[portnum -1]);

			portMac[portnum-1].port = portnum;
			sprintf(portMac[portnum-1].mac, "%02x:%02x:%02x:%02x:%02x:%02x", (mac12 & 0xff00)>>8, mac12 & 0x00ff, (mac34 & 0xff00)>>8, mac34 & 0x00ff, (mac56 & 0xff00)>>8, mac56 & 0x00ff);

			status = mdio_read (skfd, ifr, 0x10+portnum-1, 0x00);
			usleep(MDIO_RW_DELAY);
			// We check to see if this port has cable plugged in, if no, delete old records
			if( !(status & MY_LINK)) {
				log_INFO("=================================================");
				log_INFO("clear mac[%d] for !(status & MY_LINK)", portnum-1);
				log_INFO("=================================================");
				portMac[portnum-1].mac12 =0;
				portMac[portnum-1].mac34 =0;
				portMac[portnum-1].mac56 =0;
				portMac[portnum-1].mac[0] = 0;
				es[portnum-1] = 0;

				// This should be an old link which has been removed, so set  its rdb variable to empty string
				sprintf (db_name, "hw.switch.port.%d.mac", portMac[portnum-1].port -1);
				if (rdb_update_string(glb.rdb_session,db_name, "", nFlags, DEFAULT_PERM))
				{
					if (err_cnt++ == 0)
						log_ERR("Unable to update database: %s", strerror(errno));
					if (err_cnt >= (4 * 5 * 60))	// Only log 1 error every minute
						err_cnt = 0;
				}
				//break;
				continue;
			}

			portmap|=(int)(1<<(portnum-1));
			if(mac12 != portMac[portnum-1].mac12  ||  mac34 != portMac[portnum-1].mac34 ||  mac56 !=portMac[portnum-1].mac56) {
				log_INFO ("mac12 0x%x != portMac[%d].mac12 0x%x?", mac12, portnum-1, portMac[portnum-1].mac12);
				log_INFO ("mac34 0x%x != portMac[%d].mac34 0x%x?", mac34, portnum-1, portMac[portnum-1].mac34);
				log_INFO ("mac56 0x%x != portMac[%d].mac56 0x%x?", mac56, portnum-1, portMac[portnum-1].mac56);

				// check multiple times for down-->up transition before triggering dnsmasq template
				// because if different DHCP client were connected to same port before, PHY remembers these addresses and
				// these mac addresses sometimes appear in turns as valid newest addresses, especially during transition period.
				//if (portMac[portnum-1].mac12 == 0 && portMac[portnum-1].mac34 == 0 && portMac[portnum-1].mac56 == 0) {
					plugin_cnt[portnum-1]++;
					//log_INFO ("rising edge count[%d] %d, not_first[%d] %d", portnum-1, plugin_cnt[portnum-1], portnum-1, not_first[portnum-1]);
					//if (plugin_cnt[portnum-1] < 2 && not_first[portnum-1] == 1) {
					//if (plugin_cnt[portnum-1] < 4 && not_first[portnum-1] == 1) {
					//	log_INFO ("check count[%d] = %d, skip triggering", portnum-1, plugin_cnt[portnum-1]);
					//	continue;
					//}
				//} else {
					/* if mac address changes, simply ignore to allow transition from plug-out to plug-in for NTC-8000. */
				//	continue;
				//}
				//not_first[portnum-1] = 1;
				plugin_cnt[portnum-1] = 0;
				portMac[portnum-1].mac12=mac12;
				portMac[portnum-1].mac34=mac34;
				portMac[portnum-1].mac56=mac56;

				// A new MAC is added, so re-launch dnsmasq
				dhcp_restart_flag[portnum-1] = 1;
				log_INFO("set dhcp_restart flag[%d] to 1", portnum-1);
			}
			log_INFO ("Port %d 's MAC =  %s", portMac[portnum-1].port , portMac[portnum-1].mac);
		}
	}

	// determine whether trigger dnsmasq template or not
	for (i = 0; i < NUM_ETH_PORTS; i++) {
		// if found more than one valid mac address on same port, ignore until PHY get stable
		if (dhcp_restart_flag[i]) {
			if (found_valid_mac[i] >= 1) {
				sprintf (db_name, "hw.switch.port.%d.mac", portMac[i].port -1);
				(void) memset(db_mac, 0x00, 32);
				if(rdb_get_string(glb.rdb_session,db_name, db_mac, 32) >= 0 && strcmp(portMac[i].mac, db_mac) == 0) {
					log_INFO ("new mac[%d]= %s, DB value=%s", i, portMac[i].mac, db_mac);
					log_INFO ("same mac[%d] address with DB, skip setting dhcp_restart", i);
					log_INFO ("--- portMac[%d] =  %s", i, portMac[i].mac);
					continue;
				}
				dhcp_restart = 1;
				log_INFO ("rdb_update_string(glb.rdb_session,%s, %s)", db_name, portMac[i].mac);
				if (rdb_update_string(glb.rdb_session,db_name, portMac[i].mac, nFlags, DEFAULT_PERM))
				{
					if (err_cnt++ == 0)
						log_ERR("Unable to update database: %s", strerror(errno));
					if (err_cnt >= (4 * 5 * 60))	// Only log 1 error every minute
						err_cnt = 0;
				}
				/* delete same mac address in other port history because when moving
				 * same LAN cable to other port, temporaly same mac address exists in
				 * two ports' history */
				for (j = 0; j < NUM_ETH_PORTS; j++) {
					if (j == i) continue;
					if (strcmp(portMac[i].mac, portMac[j].mac) == 0) {
						portMac[j].mac12=0;
						portMac[j].mac34=0;
						portMac[j].mac56=0;
						portMac[j].mac[0] = 0;
						log_INFO("clear mac[%d] for duplicated mac", j);
					}
				}
			//} else if (found_valid_mac[i] > 1) {
			//	dhcp_restart = 0;
			//	plugin_cnt[i] = 0;
			//	portMac[i].mac12=0;
			//	portMac[i].mac34=0;
			//	portMac[i].mac56=0;
			//	portMac[i].mac[0] = 0;
			//} else {
			//	dhcp_restart = 0;
			}
		}
		log_INFO ("--- portMac[%d] =  %s", i, portMac[i].mac);
	}

	log_INFO("dhcp_restart = %d", dhcp_restart);
	if(dhcp_restart)
	{
		log_ERR("Restart dnsmasq, due to new client plug in");
		sprintf(portmapstr, "%d", portmap);
		if (rdb_update_string(glb.rdb_session,"hw.switch.port.map", portmapstr, nFlags, DEFAULT_PERM))
		{
			if (err_cnt++ == 0)
				log_ERR("Unable to update database: %s", strerror(errno));
			if (err_cnt >= (4 * 5 * 60))
				err_cnt = 0;
		}

		//if (rdb_update_string(glb.rdb_session,"service.dhcp.ipbind.enable", "1", nFlags, DEFAULT_PERM))
		//{
			//if (err_cnt++ == 0)
				//log_ERR("Unable to update database: %s", strerror(errno));
			//if (err_cnt >= (4 * 5 * 60))
				//err_cnt = 0;
		//}./common/sbin/reset_phy_link.sh

		if (rdb_update_string(glb.rdb_session,"service.dhcp.trigger", "1", nFlags, DEFAULT_PERM))
		{
			if (err_cnt++ == 0)
				log_ERR("Unable to update database: %s", strerror(errno));
			if (err_cnt >= (4 * 5 * 60))
				err_cnt = 0;
		}
	}
}


#endif

void get_all_registers(int skfd, struct ifreq *ifr, char *arg)
{
	int val;
	int regnum;
	int i;

	regnum=0;
	sscanf(arg,"%i",&regnum);

	//read the register
	for (i = 0; i < 32; i++) {
		val = mdio_read(skfd, ifr, i, regnum);
		usleep(MDIO_RW_DELAY);

		printf("get MDIO [0x%02x.0x%02x] (%d.%d) => 0x%04x\n", i, regnum, i, regnum, val);
	}
}

void get_register_data(int skfd, struct ifreq *ifr, char *arg)
{
	int val;
	int address;
	int regnum;
	int taken;

	/* get address and regnum */
	taken=sscanf(arg,"%i.%i",&address,&regnum);
	if(taken!=2) {
		fprintf(stderr,"incorrect argument format\n");
		exit(-1);
	}

	//read the register
	val = mdio_read(skfd, ifr, address, regnum);
	usleep(MDIO_RW_DELAY);

	printf("get MDIO [0x%02x.0x%02x] (%d.%d) => 0x%04x\n", address, regnum, address, regnum, val);

}

void set_register_data(int skfd, struct ifreq *ifr, char *arg)
{
	int status;
	int address;
	int regnum;
	int val;

	int taken;

	/* get address and regnum */
	taken=sscanf(arg,"%i.%i.%i",&address,&regnum,&val);
	if(taken!=3) {
		fprintf(stderr,"incorrect argument format\n");
		exit(-1);
	}

	printf("set MDIO [0x%02x.0x%02x] (%d.%d) <= 0x%04x\n", address, regnum, address, regnum, val);

	status = mdio_write(skfd, ifr, address, regnum, val);
	usleep(MDIO_RW_DELAY);
}

int main(int argc, char **argv)
{
	int	ret=0;
	int	be_daemon= 1;
	int query_flag = 0;
	int get_all_reg = 0;
	int set_flag = 0;
	char query[ARGMAX];
	#if (defined V_ETH_PORT_8plllllllw_l) && (!defined MODE_recovery)
	int updateMac_flag=0;
	#endif
	int	i;

	int	 skfd = -1;	 // AF_INET socket for ioctl() calls.
	char	*ifname;
	struct ifreq				ifr;
	struct mii_ioctl_data	*mii_data;

	glb.verbosity = 0;
	glb.rdb_session = 0;

	// Parse Options
	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'a': get_all_reg = 1; strncpy((char *)&query, optarg, ARGMAX); break;
			case 'd': be_daemon = 0; break;
			case 'q': query_flag = 1; strncpy((char *)&query, optarg, ARGMAX); break;
			case 's': set_flag = 1; strncpy((char *)&query, optarg, ARGMAX); break;
			case 'v': glb.verbosity++ ; break;
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0],
					VER_MJ, VER_MN, VER_BLD	);
				break;
			case '?': usage(argv); return 2;
		}
	}

	// Initialize the logging interface
	log_INIT(DAEMON_NAME, be_daemon);
	//openlog( DAEMON_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5 );
	//syslog( LOG_INFO, "starting" );

	be_daemon=be_daemon && !(get_all_reg|query_flag|set_flag);

	if (be_daemon)
	{
		daemonize( "/var/lock/subsys/" DAEMON_NAME, RUN_AS_USER );
		log_INFO("daemonized");
	}


	// Configure signals
	glb.run = 1;
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGCHLD, sig_handler);
	signal(SIGUSR2, sig_handler);

	if ((rdb_open(NULL, &glb.rdb_session) != 0) ||
		(glb.rdb_session == 0)) {

		pabort("can't open database device");
		/* pabort shouldn't return, but just in case. */
		return -1;
	}

	// Open a socket.
#ifdef V_IOBOARD_kudu
	ifname = "dummy0";
#else
	ifname = "eth0";
#endif
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

	mii_data = (struct mii_ioctl_data *)(&ifr.ifr_data);
	mii_data->phy_id = 0;
	mii_data->reg_num = 0;
	if ((skfd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
	{
		perror("socket");
		return 1;
	}

	/*
	 * The interface may not have come up during booting.
	 * Retry before giving up.
	 * 30 seconds is chosen for retrying period here due to Serpent Myna
     * takes around 12 seconds.
	 */
	i = 0;
	while (ioctl(skfd, SIOCGMIIPHY , &ifr) < 0)
	{
		log_ERR("SIOCGMIIPHY on %s failed: %s", ifname,
                strerror(errno));
		i++;
		if (i >= 30) {
		    log_ERR("Failed retrying SIOCGMIIPHY");
		    (void) close(skfd);
		    return 1;
		} else {
		    log_INFO("Retrying ...");
		    sleep(1);
		}
	}
	log_INFO("Interface %s is using PHY %d.",	ifname, mii_data->phy_id);


#define AT_GPIO_MEM			0xfffff000
#define AT_GPIO_MEM_LEN	4096

	if (query_flag == 1) {
		get_register_data(skfd, &ifr, (char *)&query);
		shutdown_d(0);
		return 0;
	}

	if (set_flag == 1) {
		set_register_data(skfd, &ifr, (char *)&query);
		shutdown_d(0);
		return 0;
	}

	if (get_all_reg == 1) {
		get_all_registers(skfd, &ifr, (char *)&query);
		shutdown_d(0);
		return 0;
	}

#if (defined V_ETH_PORT_8plllllllw_l) && (!defined MODE_recovery)
	char *pIpbind;
	pIpbind=(char *)rdb_get_str("service.dhcp.ipbind.enable");
	updateMac_flag = strcmp(pIpbind, "0");
#endif

	while (glb.run)
	{
		fd_set ethfds;
		int selected, nfds;
		struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
		FD_ZERO( &ethfds );
		FD_SET( skfd, &ethfds );
		nfds = 1 + skfd;
		selected = select( nfds, NULL, NULL, &ethfds, &timeout );
		for (i = 0; i < NUM_ETH_PORTS; i++) {
			update_port_status(skfd, &ifr, i);
		}

#if defined (V_SIMULTANEOUS_MULTIPLE_WWAN_VLAN_y) && defined (USE_SYSFS)
		update_vlan_port_status();
#endif

#if (defined V_ETH_PORT_8plllllllw_l) && (!defined MODE_recovery)
		if(updateMac_flag != 0){
			update_port_mac_map(skfd, &ifr);
		}
#endif

		sleep(1);
	}

	shutdown_d (0);
	return 0;
}



/*
* vim:ts=4:sw=4:
*/
