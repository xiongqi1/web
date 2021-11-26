#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>

static int signaled=0;

void signal_handler(int signal)
{
	signaled=1;
}

int run_command(char* cmd)
{
	return system(cmd);
}

int main(int argc,char* argv[])
{
	char* fname=basename(argv[0]);
	char* cmd=argv[1];

	if(argc!=2 || strcmp(argv[1],"-h")==0 || strcmp(argv[1],"--help")==0)
	{
		fprintf(stderr,"%s - run on termination\n",fname);
		fprintf(stderr,"\n");
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"\t %s <command line to run on term signal>\n",fname);
		fprintf(stderr,"\n");
		
		exit(1);
	}

	signal(SIGINT, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);

	if(signaled)
		run_command(cmd);
	else
	{
		select(0,NULL,NULL,NULL,NULL);
		run_command(cmd);
	}

	return 0;
}
