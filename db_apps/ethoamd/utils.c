#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "utils.h"

#define USPES	1000000
#define MSPES	1000
#define USPEMS	1000

// convert string into data
// if string, start with 0x/0X, treat it as hex string
// eg: ""->invalid, "10000000000000"-> invalid(errno == ERANGE) " 111" ->OK
// "100" ->OK " 0" -OK
//err out -- 0-- OK, <0 error
unsigned long Atoi( const char *pStr, int *err)
{
    long result;
    char*pEnd;
    if(err)*err =0;
    // long int strtoul(const char *sptr, char **endptr, int base);
	// if the  third parameter is 0,
    // it strtol will attempt to pick the base from pStr. Only Dec, Oct Hex supported.
    errno=0;// must be reset, otherwise, it may remain as error, even if the conversion success
	result = strtoul(pStr, &pEnd, 0);
	if(errno == ERANGE || ( pStr == pEnd))
	{
		if(err)*err = -1;
	}
    return result;
}


// call gettimeofday, convert to int64_t
int64_t gettimeofday64()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec*USPES+tv.tv_usec;
}

// call gettimeofday, convert to ms
int gettimeofdayMS()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int)tv.tv_sec*MSPES+tv.tv_usec/USPEMS;
}

// get local mac address and the interface index
//$0 --- success
// <0 -- failed
int get_local_mac( u_char *addr, int *ifIndex, const char *if_name)
{
    /* implementation for Linux */
    struct ifreq ifr, ifri;
    int s;
    int ok = 0;

    //s = socket(AF_INET, SOCK_DGRAM, 0);
    s = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (s==-1)
    {
        return -1;
    }
    if(if_name)
    {
        strcpy(ifr.ifr_name, if_name);
        strcpy(ifri.ifr_name, if_name);

        if (ioctl(s, SIOCGIFHWADDR, &ifr) == 0 && ioctl(s,  SIOCGIFINDEX, &ifri) ==0)
        {

            ok = 1;

            //NTCSLOG_DEBUG("%s Mac:  ", ifr.ifr_name);
            //print_mac((unsigned char*)ifr.ifr_hwaddr.sa_data);
            //break;
        }
    }

    close(s);
    if (ok)
    {
        bcopy( ifr.ifr_hwaddr.sa_data, addr, 6);
        *ifIndex = ifri.ifr_ifindex;
    }
    else
    {
        return -1;
    }
    return 0;
}



/*--------------------------------------------------------------------------*/
/* A simple utility routine that converts a MAC-address character string of
 * the form "12:34:56:78:AB:CD" to a 6-byte data array.
 */
int str2MAC(unsigned char *macAdr, const char *str)
{
	unsigned short b[6];
	if(strlen(str) ==0)
	{
		memset(macAdr, 0, 6);
		return 1;
	}
	if(sscanf(str,"%hx:%hx:%hx:%hx:%hx:%hx", &b[0],
			&b[1],&b[2],&b[3],&b[4],&b[5]) != 6 )
	{
		return 0;
	}
	macAdr[0]= b[0];
	macAdr[1]= b[1];
	macAdr[2]= b[2];
	macAdr[3]= b[3];
	macAdr[4]= b[4];
	macAdr[5]= b[5];

	//print_mac(str, macAdr);
    return(1);
}

void MAC2str(char *str, const unsigned char *mac)
{
	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",  mac[0],
			 mac[1],mac[2],mac[3],
			 mac[4],mac[5]);

}
