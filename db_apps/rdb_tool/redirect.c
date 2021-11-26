#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void print_usage(void)
{
    fprintf(stderr,"Usage: redirect <source> <executable binary> [arguments]"
                   " ... \n");
    fprintf(stderr,"Redirects all of stdin/stdout/stderr to be the source"
                   " file\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "arguments: a list of arguments to pass to the target"
                    " binary.\n");
}

int main(int argc,char* argv[])
{
	const char* new_inout;
	const char* exec_bin;
	
	int target_argc;
	char* target_argv[256];
	int i;

	int stat;

	int fd_in;
	int fd_out;

	if(argc<2)
	{
		print_usage();
		exit(-1);
    } else if (argc == 2) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            print_usage();
            exit(0);
        }
    }

	new_inout=argv[1];
	exec_bin=argv[2];

	// get taret argc
	target_argc=argc-2;

	// build target argv
	i=0;
	while((i<target_argc) && (i < (sizeof(target_argv) / sizeof(target_argv[0]) - 1)))
	{
		target_argv[i]=argv[2+i];
		i++;
	}
	target_argv[i]=0;

	// open file
	fd_in=open(new_inout,O_RDONLY | O_TRUNC);
	if(fd_in<0)
	{
		fprintf(stderr,"failed to open %s - %s\n",new_inout,strerror(errno));
		exit(-1);
	}

	// write file
	fd_out=open(new_inout,O_WRONLY | O_TRUNC);
	if(fd_out<0)
	{
		fprintf(stderr,"failed to open %s - %s\n",new_inout,strerror(errno));
		exit(-1);
	}

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	dup2(fd_in,STDIN_FILENO);
	dup2(fd_out,STDOUT_FILENO);
	dup2(fd_out,STDERR_FILENO);

	close(fd_in);
	close(fd_out);

	stat=execv(exec_bin,target_argv);

	fprintf(stderr,"execv failed - %s\n", strerror(errno));

	exit(-1);
}
