/*!
 * Copyright Notice:
 * Copyright (C) 2002-2008 Call Direct Cellular Solutions
 *
 */

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h> 
#include "string.h"

#include "rdb_util.h"

int file_to_array(int some, char* varname, FILE* outfp, char* infname)
{
  FILE* infp;
  char buf[256];
  char qbuf[256];
  int first = 1;
  char* cp;

//  fprintf(outfp, "\"%s\":[\n", varname);
  if ((infp = fopen(infname, "r")) != 0)
  {
    while (fgets(buf, sizeof(buf) - 1, infp))
    {
      if ((cp = strchr(buf, '\n')) != NULL)
        * cp = '\0';
      if ((cp = strchr(buf, '\r')) != NULL)
        * cp = '\0';
      if (buf[0] == '\0')
        continue;
      if (some)
        fprintf(outfp, ",\n");
      some = 1;
      if (first)
      {
        first = 0;
        fprintf(outfp, "\"%s\":[\n", varname);
      }
      ////quotit(qbuf, sizeof(qbuf), buf, "\"\\'");
      fprintf(outfp, "%s", qbuf);
    }
    fclose(infp);
    if (!first)
    {
      fprintf(outfp, "]\n");
    }
  }
// fprintf(outfp, "]\n");
  return some;
}

int listWriter( void* param )
{
  char** list;
  int argn;
  list = param;
  printf("{\n");

  for (argn = 0;list[argn] && list[argn+1];argn += 2)
  {
	  printf( "%s:%s,\n", list[argn], list[argn+1]);
  }
  printf(" }\n");
  return 1;
}
/*
void setList(char* filename, char* date, char* size )
{
  char* list[8];
  char tb[8];
  char lb[8];
  int i;
	
  if (*date)
  {
	  list[0] = "'filename'";
	  list[1] = filename;
  }
  i = 2;
  if (*date)
  {
    list[i++] = "'date'";
    list[i++] = date;
  }
  // 4 
  if ( *size)
  {
    list[i++] = "'size'";
    list[i++] = size;
  }
  // 6 
<<<<<<< .mine
  if (action && * action)
  {
    list[i++] = "'action'";
    list[i++] = action;
  }
  // 8 
 // if (lbr )
  {
    list[i++] = "'last_bytes_read'";
	sprintf(lb, "%ld", lbr);
    list[i++] = lb;
  }
  // 10 
=======
>>>>>>> .r3173
  list[i++] = NULL;
  list[i++] = NULL;

  listWriter( list );
}
*/



int main(int argc, char* argv[], char* envp[])
{
	#define MYBUFFER_SIZE 32
	char *str;
	char buff[16];
	int i, j;
	//int some = 0;
	pid_t cpid;

	openlog("AppUpload", LOG_PID | LOG_NDELAY, LOG_DAEMON);
	if (rdb_start())
	{
		syslog(LOG_ERR, "can't open RDB driver %i ( %s )\n", -errno, strerror(errno));
	}
//	cpid = fork();
	if (cpid == -1) { perror("fork"); exit(EXIT_FAILURE); }
	if (cpid == 0)// Code executed by child 
	{ 
//  printf("Child PID is %ld\n", (long) getpid());
//system("/www/upload/upload.esp");
		syslog(LOG_ERR, "AppUpload-0\n");

		sprintf( buff, "flashtool /opt/%s\n", get_single("upload.client_filename"));            
		syslog(LOG_ERR, buff);
		system(buff);
		system("rm /opt/*\n");
		set_single("upload.current_size", "-2");
    	}
	else
	{  
		printf("Content-Type: text/html\n\n");

		str = getenv("QUERY_STRING");
		if (str)
		{
			system( "ls -l /opt >/tmp/uploaded_files" );
			if( strstr( str, "g=formS" ) )
			{
			/*	printf("var uploaded_files = {\n");
				some = file_to_array(some, "messages", stdout, "/tmp/uploaded_files");
				printf(" };\n");
				some = 0;*/
				printf("var uploaded_files = {\n");
			/*	if (!(some = file_to_array(some, "messages", stdout, "/tmp/uploaded_files")))
				{
				  fprintf(stdout, "\"messages\":[,]\n");
				}*/
				printf(" };\n");
			}
		}
		else
		{
			syslog(LOG_ERR, "AppUpload-1\n");
			j = (int)( atol( get_single("upload.file_size") ) / 1000000 ) + 1;
			sprintf( buff, "%u", j*1024);
			set_single( "upload.flash_time", buff );
			sprintf( buff, "j=%u", j*1024 );
			syslog(LOG_ERR, buff);
			for( i=0; i<=j; i++ )
			{
				sprintf( buff, "%u", i);
				set_single( "upload.flash_position", buff );
				syslog(LOG_ERR, buff);
				usleep(1000000);
			}
			usleep(2000000);
		}
	}
	rdb_end();
	exit(0);	
}


