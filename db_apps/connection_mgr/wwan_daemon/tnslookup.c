/*

	tiny nslookup module
	
	26 Aug 2001 
	Yong

*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <syslog.h>

#define	DNS_MAX			1025	/* Maximum host name		*/
#define	DNS_PACKET_LEN		2048	/* Buffer size for DNS packet	*/

struct header {
	uint16_t	tid;		/* Transaction ID		*/
	uint16_t	flags;		/* Flags			*/
	uint16_t	nqueries;	/* Questions			*/
	uint16_t	nanswers;	/* Answers			*/
	uint16_t	nauth;		/* Authority PRs		*/
	uint16_t	nother;		/* Other PRs			*/
	unsigned char	data[1];	/* Data, variable length	*/
};

enum dns_query_type {
	DNS_A_RECORD = 0x01,		/* Lookup IP adress for host	*/
	DNS_MX_RECORD = 0x0f		/* Lookup MX for domain		*/
};

static int _sock=-1;

int rcvbufsiz = 128 * 1024;

struct sockaddr_in _sa;
char _dev[256];

void tnslookup_fini()
{
	if(_sock<0)
		return;
	
	close(_sock);
	_sock=-1;
}

int tnslookup_isinit(void)
{
	return _sock>=0; 
}

int tnslookup_init(const char* dev) 
{
	int len;
	int stat;
	int flags;

	// open socket
	_sock=socket(PF_INET, SOCK_DGRAM, 17);
	if(_sock<0) {
		syslog(LOG_ERR,"failed in socket() - %s",strerror(errno));
		return -1;
	}
	
	// increase recieve buffer size
	setsockopt(_sock, SOL_SOCKET, SO_RCVBUF,(char *) &rcvbufsiz, sizeof(rcvbufsiz));
	
	
	// bypass if device not specified
	if(!dev || !dev[0]) {
		syslog(LOG_INFO,"network device not specified - not binding");
		return 0;
	}
	
	// store network device
	strncpy(_dev,dev,sizeof(_dev));
	_dev[sizeof(_dev)-1]=0;

	len = strlen(_dev);

	// bind to the specific interface
	stat = setsockopt(_sock, SOL_SOCKET, SO_BINDTODEVICE, _dev, len+1);
	if (stat < 0)
	{
		syslog(LOG_ERR, "failed to bind to interface - dev=%s,err=%s", _dev, strerror(errno));
		return -1;
	}

	// set to nonblocking.
	if ((flags = fcntl(_sock, F_GETFL, 0)) >= 0) {
		if (0 != fcntl(_sock, F_SETFL, flags | O_NONBLOCK))
			syslog(LOG_ERR, "failed to set to NONBLOCK err=%s", strerror(errno));
	}

	return 0;
}

void tnslookup_change_dns_server(const char* dns_server)
{
	// set server address 
	inet_aton(dns_server,&_sa.sin_addr);
	_sa.sin_family	= AF_INET;
	_sa.sin_port	= htons(53);
}

static int tnslookup_parse_udp(const unsigned char *pkt, int len)
{
	struct header		*header;
	const unsigned char	*p, *e, *s;
	uint16_t		type;
	int			found, stop, dlen, nlen;
	unsigned char addr[DNS_MAX];
	int addr_len;

	/* We sent 1 query. We want to see more that 1 answer. */
	header = (struct header *) pkt;
	if (ntohs(header->nqueries) != 1) {
		syslog(LOG_ERR,"DNS query count not matched - queries=%d",ntohs(header->nqueries));
		return -1;
	}

	/* Received 0 answers */
	if (header->nanswers == 0) {
		syslog(LOG_ERR,"DNS does not exist - answers=%d",header->nanswers);
		return -1;
	}

	/* Skip host name */
	for (e = pkt + len, nlen = 0, s = p = &header->data[0];
		    p < e && *p != '\0'; p++)
		nlen++;

#define	NTOHS(p)	(((p)[0] << 8) | (p)[1])

	/* We sent query class 1, query type 1 */
	if (&p[5] > e || NTOHS(p + 1) != DNS_A_RECORD) {
		syslog(LOG_ERR,"qtype not matched - qtype=%d",NTOHS(p + 1));
		return -1;
	}

	/* Go to the first answer section */
	p += 5;

	/* Loop through the answers, we want A type answer */
	for (found = stop = 0; !stop && &p[12] < e; ) {

		/* Skip possible name in CNAME answer */
		if (*p != 0xc0) {
			while (*p && &p[12] < e)
				p++;
			p--;
		}

		type = htons(((uint16_t *)p)[1]);

		if (type == 5) {
			/* CNAME answer. shift to the next section */
			dlen = htons(((uint16_t *) p)[5]);
			p += 12 + dlen;
		} else if (type == DNS_A_RECORD) {
			found = stop = 1;
		} else {
			stop = 1;
		}
	}

	if (found && &p[12] < e) {
		dlen = htons(((uint16_t *) p)[5]);
		p += 12;

		if (p + dlen <= e) {
			
			// get addr
			addr_len=dlen;
			if(addr_len>sizeof(addr)-1)
				addr_len=sizeof(addr)-1;
			memcpy(addr,p,addr_len);
			addr[addr_len]=0;
			
			syslog(LOG_INFO,"resolved - addr=%d.%d.%d.%d",(unsigned)addr[0],(unsigned)addr[1],(unsigned)addr[2],(unsigned)addr[3]);
			return 0;
		}
	}
	
	syslog(LOG_ERR,"CNAME not found");
	
	return -1;
}
			
int tnslookup_recv()
{
	struct sockaddr_in	sa;
	socklen_t		len = sizeof(_sa);
	int			n;
	unsigned char		pkt[DNS_PACKET_LEN];
	
	if(!tnslookup_isinit())
		return -1;

	/* Check our socket for new stuff */
	while ((n = recvfrom(_sock, pkt, sizeof(pkt), 0,(struct sockaddr *) &sa, &len)) > 0 &&  n > (int) sizeof(struct header)) {
		if(tnslookup_parse_udp(pkt, n)>=0)
			return 0;
	}
	
	return -1;
}

int tnslookup_isready()
{
	fd_set set;
	
	if(!tnslookup_isinit())
		return 0;
	
	FD_ZERO(&set);
	FD_SET(_sock, &set);

	struct timeval tv={0, 0};
	
	return select(_sock + 1, &set, NULL, NULL, &tv) == 1;
}

int tnslookup_send(const char* name)
{
	struct header *header;
	unsigned char pkt[DNS_PACKET_LEN];
	int i;
	int n;
	int name_len;
	char *p;
	const char *s;
	
	static uint16_t tid=0;
	
	if(!tnslookup_isinit())
		return -1;
	
	// prepare dns header	
	header		= (struct header *) pkt;
	header->tid	= ++tid;
	header->flags	= htons(0x100);		/* Haha. guess what it is */
	header->nqueries= htons(1);		/* Just one query */
	header->nanswers= 0;
	header->nauth	= 0;
	header->nother	= 0;
	
	/* Encode DNS name */

	name_len = strlen(name);
	p = (char *) &header->data;	/* For encoding host name into packet */

	do {
		if ((s = strchr(name, '.')) == NULL)
			s = name + name_len;

		n = s - name;			/* Chunk length */
		*p++ = n;			/* Copy length */
		for (i = 0; i < n; i++)		/* Copy chunk */
			*p++ = name[i];

		if (*s == '.')
			n++;

		name += n;
		name_len -= n;

	} while (*s != '\0');

	*p++ = 0;			/* Mark end of host name */
	*p++ = 0;			/* Well, lets put this byte as well */
	*p++ = (unsigned char)DNS_A_RECORD;	/* Query Type */

	*p++ = 0;
	*p++ = 1;			/* Class: inet, 0x0001 */

	n = (size_t)p - (size_t)pkt; /* Total packet length */


	if (sendto(_sock, pkt, n, 0, (struct sockaddr *) &_sa, sizeof(_sa)) != n) {
		syslog(LOG_ERR,"failed in sendto() - %s",strerror(errno));
		
		// close the handle - we assume the interface is removed
		tnslookup_fini();
		return -1;
	}
	
	return 0;
}

