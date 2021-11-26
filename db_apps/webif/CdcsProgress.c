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

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include "string.h"

#include "rdb_util.h"

int splitat(char* buf, char** newptr, char** ptrs, int nitems, char atchar)
{
  char* pPtr = buf;

  int cItemCnt = 0;

  // clear all seperators
  while (*pPtr)
  {
    if (*pPtr == atchar || *pPtr == '\r' || *pPtr == '\n' || *pPtr == ' ')
      *pPtr = 0;
    pPtr++;
  }

  pPtr = buf;

  while (cItemCnt < nitems)
  {
    // skip blanks
    while (!*pPtr)
      pPtr++;

    // get token
    *ptrs++ = pPtr;

    // skip letters
    while (*pPtr)
      pPtr++;

    cItemCnt++;
  }

  // skip blanks
  while (!*pPtr)
    pPtr++;

  // get token
  *newptr = pPtr;

  return cItemCnt;
}


char* quotit(char* outquote, int maxlen, char* inquote, char* qlist)
{
  int i;
  if (maxlen < 3)
  {
    outquote[0] = '\0';
    return outquote;
  }
  outquote[0] = '"';
  i = 1;
  while (*inquote != '\0')
  {
    if (qlist && strchr(qlist, * inquote))
    {
      if (i >= maxlen - 3)
        break;
      outquote[i++] = '\\';
      outquote[i++] = * inquote++;
      continue;
    }
    else
    {
      if (i >= maxlen - 2)
        break;
      outquote[i++] = * inquote++;
      continue;
    }
  }
  outquote[i++] = '"';
  outquote[i] = '\0';
  return outquote;
}


int file_to_array(int some, char* varname, FILE* outfp, char* infname)
{
  FILE* infp;
  char buf[256];
  char qbuf[256];
  int first = 1;
  char* cp;

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
      quotit(qbuf, sizeof(qbuf), buf, "\"\\'");
      fprintf(outfp, "%s", qbuf);
    }
    fclose(infp);
    if (!first)
    {
      fprintf(outfp, "]\n");
    }
  }
  return some;
}

int isNumber(const char* cp)
{
  int ini = 1;
  int plusmok = 1;
  int doted;
  int expok = 1;
  doted = 0;
  while (*cp)
  {
    if (ini && isspace(*cp))
    {
      cp++;
      continue;
    }
    if (plusmok && (*cp == '-' || * cp == '+'))
    {
      ini = 0;
      plusmok = 0;
      cp++;
      continue;
    }
    if (expok && (*cp == 'e' || * cp == 'E'))
    {
      ini = 0;
      plusmok = 1;
      expok = 0;
      cp++;
      continue;
    }
    ini = 0;
    if (isdigit(*cp))
    {
      cp++;
      plusmok = 0;
      continue;
    }
    if (*cp == '.')
    {
      if (doted)
        return 0;
      doted = 1;
      plusmok = 0;
      cp++;
      continue;
    }
    return 0;
  }
  return 1;
}

char errbuffer[64];

int listWriter( void* param )
{
  char** list;
  int argn;
  list = param;
  printf("{\n");

  for (argn = 0;list[argn] && list[argn+1];argn += 2)
  {
   // fprintf(fp, "%s:%s\n", list[argn], list[argn+1]);
	  if(argn > 0) printf(",\n");
	  printf( "%s:%s", list[argn], list[argn+1]);
  }
  printf("}\n");
  return 1;
}


void setProgress(long content_len, char* title, char* phase, char* action, long lbr)
{
  char* list[12];
  char tb[12];
  char lb[12];
  int i;
  sprintf(tb, "%ld", content_len);
  list[0] = "'total_bytes'";
  list[1] = tb;

  i = 2;
  if (title && * title)
  {
    list[i++] = "'title'";
    list[i++] = title;
  }
  /* 4 */
  if (phase && * phase)
  {
    list[i++] = "'phase'";
    list[i++] = phase;
  }
  /* 6 */
  if (action && * action)
  {
    list[i++] = "'action'";
    list[i++] = action;
  }
  /* 8 */
 // if (lbr )
  {
    list[i++] = "'last_bytes_read'";
	sprintf(lb, "%ld", lbr);
    list[i++] = lb;
  }
  /* 10 */
  list[i++] = NULL;
  list[i++] = NULL;

  listWriter( list );
 // makeProgressFile(listWriter, list);
}




int progress()
{
  char buf[256];
  FILE* fp;
  char* pos;
  long lbr = 0;
  long len;

	
	system( "ls -l /opt/ >/tmp/upload.inf" );

	if((fp = fopen("/tmp/upload.inf", "r")) != 0)
    {
		while(fgets(buf, sizeof(buf), fp) != NULL)
		{
			//if( strstr(buf, get_single("upload.local_filename") ))
			{
				if( (pos=strstr(buf,"root" )) != NULL )
				{
					while( *pos && !isdigit(*pos) )pos++;
					if(*pos)
					{
						lbr = atol(pos);
						len = atol(get_single("upload.file_size") );
						sprintf( buf, get_single("upload.local_filename") );
						setProgress( len, "'Upload'", "'upload'", "'start'", lbr);
						break;
					}
				}
			}
		/*	else if( strstr(buf, get_single("upload.client_filename") ))
			{
				setProgress(-1L, "'Unpacking Files'", "'unpack'", "'info'", NULL);
			}
			else
			{
				//setProgress(-1L, "'Finished'", "'complete'", "'stop'", NULL);	
			}	*/
		}
		
	}
	

	return (0);
}


int main(int argc, char* argv[], char* envp[])
{
  //char buf[256];
  //char* pos;
  long lbr = 0;
  long len;

	openlog("progress", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	if(rdb_start())
	{
		syslog(LOG_ERR, "can't open cdcs_DD %i ( %s )\n", -errno, strerror(errno));
	}
	printf("Content-Type: text/html\n\n");
/*	pos = getenv("QUERY_STRING");
	if (pos)
	{
		//set_single( "fileQ", pos );
		if( strncmp(pos, "install", 7 )==0 )
		{
			sprintf( buf,"var installFile=\"%s\";\n",get_single("installFile") );
			printf( buf );
			exit(0);
		}
	}*/

	lbr = atol(get_single("upload.current_size") );
	if( lbr>=0 )
	{
		len = atol(get_single("upload.file_size") );
		setProgress( len, "'Upload'", "'upload'", "'start'", lbr);
	}
/*	else if( lbr == (-1) )
	{
		
		lbr = atol(get_single("upload.flash_position") );
		len = atol(get_single("upload.flash_time") );
len = 9*1025;
		setProgress( len, "'Unpacking Files'", "'unpack'", "'info'", lbr*1024);
	//	sprintf( buf, get_single("upload.local_filename") );
	}*/
	else
	{
		setProgress(-1L, "'Finished'", "'complete'", "'stop'", 0 );	
	}
//syslog(LOG_ERR, "progress-1x");
//	progress();
	rdb_end();
	exit(0);
}


