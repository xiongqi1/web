#include <stdio.h>

#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

void printUsage()
{
	fprintf(stderr, "Flush the conntrack table on the netfilter\n");
	fprintf(stderr, "\nUsage: \n\t flush_conntrack_cache \n");
}

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ 0, 0, 0, 0 }
};

int main(int argc,char* argv[])
{
	static struct nfct_handle *cth;
	int family = AF_INET;	
	int res = 0;
	int opt;

	while ((opt = getopt_long(argc, argv, "h", long_opts, NULL)) != EOF)
	{
		switch (opt)
		{
			case 'h':
				printUsage();
				return -1;
		}
	}

	cth = nfct_open(CONNTRACK, 0);
	if (!cth)
		fprintf(stderr,"Can't open conntrack handler");
	
	res = nfct_query(cth, NFCT_Q_FLUSH, &family);
	if(res<0) {
		fprintf(stderr,"failed to flush - %s\n",strerror(errno));
		nfct_close(cth);
		return -1;
	}
	
	nfct_close(cth);
	fprintf(stdout,"connection tracking table has been emptied.\n");
	
	return res;
}
