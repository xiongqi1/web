/*
 *	@file 	espProcs.c
 *	@brief 	Embedded Server Pages (ESP) Procedures.
 *	@overview These ESP procedures can be used in ESP pages for common tasks.
 */
/********************************* Copyright **********************************/
/*
 *	@copy	default
 *	
 *	Copyright (c) Mbedthis Software LLC, 2003-2007. All Rights Reserved.
 *	
 *	This software is distributed under commercial and open source licenses.
 *	You may use the GPL open source license described below or you may acquire 
 *	a commercial license from Mbedthis Software. You agree to be fully bound 
 *	by the terms of either license. Consult the LICENSE.TXT distributed with 
 *	this software for full details.
 *	
 *	This software is open source; you can redistribute it and/or modify it 
 *	under the terms of the GNU General Public License as published by the 
 *	Free Software Foundation; either version 2 of the License, or (at your 
 *	option) any later version. See the GNU General Public License for more 
 *	details at: http://www.mbedthis.com/downloads/gplLicense.html
 *	
 *	This program is distributed WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	
 *	This GPL license does NOT permit incorporating this software into 
 *	proprietary programs. If you are unable to comply with the GPL, you must
 *	acquire a commercial license to use this software. Commercial licenses 
 *	for this software and support services are available from Mbedthis 
 *	Software at http://www.mbedthis.com 
 *	
 *	@end
 */
/********************************** Includes **********************************/

#include  "esp.h"
#include  "rdb_ops.h"
#include  "cdcs_base64.h"
#include  "blake2.h"
#include  <regex.h>

#define STRNCPY(dst,src,len) do { strncpy(dst,src,len); ((char*)(dst))[len-1]='\0'; } while(0)

/************************************ Code ************************************/
#if BLD_FEATURE_ESP_MODULE
#if BLD_FEATURE_SESSION
/*
 *	destroySession
 */

static int destroySessionProc(EspRequest *ep, int argc, char **argv)
{
	ep->esp->destroySession(ep->requestHandle);
	return 0;
}

#endif	/* BLD_FEATURE_SESSION */

/******************************************************************************/
/*
 *	include
 *
 *	This includes javascript libraries. For example:
 *
 *		<% include("file", ...); %> 
 *
 *	Don't confuse with ESP includes:
 *
 *		<% include file.esp %>
 *
 *	Filenames are relative to the base document including the file.
 *	FUTURE -- move back to EJS. Only here now because we need ep->readFile.
 */ 

static int includeProc(EspRequest *ep, int argc, char **argv)
{
	Esp		*esp;
	char	path[MPR_MAX_FNAME], dir[MPR_MAX_FNAME];
	char	*emsg, *buf;
	int		size, i;

	esp = ep->esp;
	mprAssert(argv);
	for (i = 0; i < argc; i++) {
		mprGetDirName(dir, sizeof(dir), ep->docPath);
		mprSprintf(path, sizeof(path), "%s/%s", dir, argv[i]);

		if (esp->readFile(ep->requestHandle, &buf, &size, path) < 0) {
			espError(ep, "Can't read include file: %s", path);
			return MPR_ERR_CANT_ACCESS;
		}
		buf[size] = '\0';

		if (ejsEvalScript(espGetScriptHandle(ep), buf, 0, &emsg) < 0) {
			espError(ep, "Cant evaluate script");
			mprFree(buf);
			return -1;
		}
		mprFree(buf);
	}
	return 0;
}

/******************************************************************************/
/*
 *	redirect
 *
 *	This implemements <% redirect(url, code); %> command. The redirection 
 *	code is optional.
 */ 

static int redirectProc(EspRequest *ep, int argc, char **argv)
{
	char	*url;
	int		code;

	if (argc < 1) {
		espError(ep, "Bad args");
		return MPR_ERR_BAD_ARGS;
	}
	url = argv[0];
	if (argc == 2) {
		code = atoi(argv[1]);
	} else {
		code = 302;
	}
	espRedirect(ep, code, url);
	return 0;
}

/******************************************************************************/
#if BLD_FEATURE_SESSION
/*
 *	useSession
 */

static int useSessionProc(EspRequest *ep, int argc, char **argv)
{
	int			timeout;

	if (argc > 1) {
		espError(ep, "Bad args");
		return MPR_ERR_BAD_ARGS;

	} else if (argc == 1) {
		timeout = atoi(argv[0]);
	} else {
		timeout = 0;
	}

	ep->esp->createSession(ep->requestHandle, timeout);
	espSetReturnString(ep, ep->esp->getSessionId(ep->requestHandle));
	return 0;
}

#endif /* BLD_FEATURE_SESSION */
/******************************************************************************/
/*
 *	setHeader
 *
 *	This implemements <% setHeader("key: value", allowMultiple); %> command.
 */ 

static int setHeaderProc(EspRequest *ep, int argc, char **argv)
{
	mprAssert(argv);
	if (argc != 2) {
		espError(ep, "Bad args");
		return MPR_ERR_BAD_ARGS;
	}
	ep->esp->setHeader(ep->requestHandle, argv[0], atoi(argv[1]));
	return 0;
}

/******************************************************************************/
/*
 *	write
 *
 *	This implemements <% write("text"); %> command.
 */ 

static int writeProc(EspRequest *ep, int argc, char **argv)
{
	char	*s;
	int		i, len;

	mprAssert(argv);
	for (i = 0; i < argc; i++) {
		s = argv[i];
		len = strlen(s);
		if (len > 0) {
			if (espWrite(ep, s, len) != len) {
				espError(ep, "Can't write to client");
				return -1;
			}
		}
	}
	return 0;
}

/******************************************************************************/
/*
 *	rename
 *
 *	This implemements <% rename(oldFile, newFile); %> command.
 */ 

static int renameProc(EspRequest *ep, int argc, char **argv)
{
	if (argc != 2) {
		espError(ep, "Bad args");
		return -1;
	}
	if (rename(argv[0], argv[1]) < 0) {
		espError(ep, "Can't rename uploaded file");
	}
	return 0;
}
/******************************************************************************/
#define MY_MAX_LIST_SIZE	2047

static int rdb_exists(EspRequest *ep, int argc, char **argv)
{
	struct rdb_session *s = NULL;
	int retval;
	int len = 0;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		if (argc > 1)
			espSetReturnString(ep, argv[1] );
		else
			espSetReturnString(ep, "" );
		return 0;
	}

	retval = rdb_getinfo(s, argv[0], &len, NULL, NULL);

	rdb_close(&s);

	if (retval == -EOVERFLOW) {
		espSetReturnString(ep, "1" );
	} else {
		espSetReturnString(ep, "" );
	}

	return 0;
}

static int get_single(EspRequest *ep, int argc, char **argv)
{
	struct rdb_session *s = NULL;
	int retval;
	char value[MY_MAX_LIST_SIZE+1];
	int len = sizeof(value);
	int base64 = 0;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}

	if (argc == 3) {
		/*
		 * argv[0]: RDB name to read
		 * argv[1]: value to return if rdb_open fails
		 * Adding argv[2]:
		 * 	--base64	do base64 encode on RDB value excluding trailing \0
		 */
		if (!strcmp(argv[2], "--base64")) {
			base64 = 1;
		}
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		if (argc > 1)
			espSetReturnString(ep, argv[1] );
		else
			espSetReturnString(ep, "" );
		return 0;
	}

	retval = rdb_get(s, argv[0], value, &len);

	rdb_close(&s);

	if (retval) {
		espSetReturnString(ep, "N/A" );
	} else {
		if (base64) {
			/* need to base64-encode */
			char *base64_out = malloc(cdcs_base64_get_encoded_len(len));
			/* exclude \0 */
			while (len > 0 && !value[len-1]) {
				len--;
			}
			if (len > 0 && cdcs_base64encode(value, len, base64_out, 0) > 0) {
				/* base64_out will be strdup-ed here */
				espSetReturnString(ep, base64_out );
			}
			else {
				espSetReturnString(ep, "" );
			}
			free(base64_out);
		}
		else {
			espSetReturnString(ep, value );
		}
	}

	return 0;
}

/*
 * this implements <% base64_encoded_string = base64_encode("string to be base64-encoded"); %> command
 * espError "Bad args" for invalid arguments
 * espSetReturnString base64-encoded string or empty string on error
 */
static int base64_encode(EspRequest *ep, int argc, char **argv)
{
	char *base64_out = NULL;
	int len;

	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}

	len = strlen(argv[0]);
	if (len && (base64_out = malloc(cdcs_base64_get_encoded_len(len)))
			&& cdcs_base64encode(argv[0], len, base64_out, 0) > 0) {
		/* base64_out will be strdup-ed here */
		espSetReturnString(ep, base64_out);
	}
	else {
		espSetReturnString(ep, "");
	}
	free(base64_out);

	return 0;
}

static int set_single_direct(EspRequest *ep, int argc, char **argv)
{
	char* perm;
	int flags;
	
	struct rdb_session *s = NULL;
	int retval;

	// check argument validation
	if (argc != 3) {
		syslog(LOG_ERR,"incorrect number of arguments used for set_single_direct() - %d",argc);
		return -1;
	}

	perm=argv[0];
	
	// build flags
	flags=0;
	if(strstr(perm,"-p"))
		flags|=PERSIST;

	retval = rdb_open(NULL, &s);
	if (retval) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}

	retval = rdb_update_string(s, argv[1], argv[2], flags, DEFAULT_PERM);

	rdb_close(&s);

	if (retval) {
		syslog(LOG_ERR,"rdb_update_string failed to update name(%s):value(%s) - %s",argv[1], argv[2],strerror(errno));
	}

	return 0;
}

static int get_single_direct(EspRequest *ep, int argc, char **argv)
{
	struct	rdb_session *s = NULL;
	int	retval;
	char	value[MY_MAX_LIST_SIZE+1];
	int	len = sizeof(value);

	if (argc != 1) {
		syslog(LOG_ERR,"incorrect number of arguments used for get_single_direct() - %d",argc);
		return -1;
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}

	retval = rdb_get(s, argv[0], value, &len);

	rdb_close(&s);

	if (retval == 0) {
		espWriteString(ep, value);
	} else {
		syslog(LOG_DEBUG,"rdb_get failed - name(%s),retval(%d)",argv[0],retval);
	}

	return 0;
}

static int set_single(EspRequest *ep, int argc, char **argv)
{
	struct	rdb_session *s = NULL;
	char	name[MAX_NAME_LENGTH+1]; 
	char	value[MY_MAX_LIST_SIZE+1];
	char	buffer[MAX_NAME_LENGTH+MY_MAX_LIST_SIZE+1];
	char	*p_pos;
	int	retval;
	int	flags = 0;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}
	retval = rdb_open(NULL, &s);
	if (retval) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}
	STRNCPY( buffer, argv[0], sizeof(buffer));
	p_pos=strchr( buffer, '=' );
	if( p_pos )
	{
		*p_pos = 0;
		STRNCPY( name, buffer, sizeof(name));
		STRNCPY( value, p_pos+1, sizeof(value));
	}
	else
	{
		STRNCPY( name, buffer, sizeof(name));
	}

	if( argc > 1 )
	{
		if(strncmp( argv[1], "-p", 2 )==0)
		{
			flags |= PERSIST;
		}
	}
	retval = rdb_update_string(s, name, value, flags, DEFAULT_PERM);

	rdb_close(&s);
	if( retval )
		printf("rdb_update_string returns %i ( %s )\n",-errno, strerror(errno));

	return 0;
}

static int get_list(EspRequest *ep, int argc, char **argv)
{
	struct	rdb_session *s = NULL;
	char	value[MY_MAX_LIST_SIZE+1];
	int	len = sizeof(value);
	int	retval;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		if (argc > 1)
			espSetReturnString(ep, argv[1] );
		else
			espSetReturnString(ep, "" );
		return 0;
	}

	*value=0;
	if(!rdb_getnames(s, argv[0], value, &len, 0) )
	{
		espSetReturnString(ep, value);
	}
	else
	{
		espSetReturnString(ep, "N/A" );
		fprintf(stderr, "rdb_getnames returns %i ( %s ) %s -- %s\n",-errno, strerror(errno),argv[0],value);
	}

	rdb_close(&s);
	return 0;
}

static int unhex( char c )
{
	return( c >= '0' && c <= '9' ? c - '0'
	    : c >= 'A' && c <= 'F' ? c - 'A' + 10
	    : c - 'a' + 10 );
}

static void unescape ( char * s)
{
	char	*p;

	for ( p = s; *s != '\0'; ++s ) {
		if ( *s == '%' ) {
			if ( *++s != '\0' ) {
				*p = unhex( *s ) << 4;
			}
			if ( *++s != '\0' ) {
				*p++ += unhex( *s );
			}
		} else {
			*p++ = *s;
		}
	}

	*p = '\0';
}

/*
 * This implements <% string = unescape_string("string"); %> command.
 * It invokes function unescape, which converts %HH, where H is a hexadecimal number, to corresponding  characters, on given string.
 * espSetReturnString unescape-ed string
 */
static int unescape_string(EspRequest *ep, int argc, char **argv)
{
	char *endptr;

	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}

	unescape(argv[0]);
	espSetReturnString(ep, argv[0]);
	return 0;
}

static int set_escapedlist(EspRequest *ep, int argc, char **argv)
{
	int	fd;
	int	retval;
	char	name[MAX_NAME_LENGTH+1];
	char	value[MY_MAX_LIST_SIZE+1];
	char	*str1, *token, *p_pos;


	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}
	ioctl_args ps={name, value, 0, 0, 0};
	if( (fd = open("/dev/cdcs_DD", O_RDWR ) ) ==  - 1)
	{
		printf( "can't open cdcs_DD %i ( %s )\n", -errno, strerror(errno));
		return -1;
	}

	for (str1 = argv[0]; ; str1 = NULL)
	{
		token = strtok(str1, "&");
		if (token == NULL)
			break;

		p_pos=strchr( token, '=' );
		if( p_pos )
		{
			*p_pos = 0;
			STRNCPY(name, token, sizeof(name));
			STRNCPY(value, p_pos+1, sizeof(value));
		}
		else
		{
			STRNCPY(name, token, sizeof(name));
			// do not delete a rdb variable. Otherwise, database driver causes a performance problem by flooding
			// signals to all processes that subscribed the variable. Most of time, rdb_manager gets hurt
		}

		unescape(value);

		// workaround for database driver - database driver is having data corruption if zero-length variable is created
		// so we put terminating zero together with the string to avoid this bug
		ps.len = strlen(ps.value)+1;
		ps.perm = DEFAULT_PERM;
		ps.flags = 0;
		if( argc > 1 )
		{
			if(strncmp( argv[1], "-p", 2 )==0)
			{
				ps.flags |= PERSIST;
			}
		}
		retval = ioctl(fd, SETSINGLE, (unsigned long)&ps );
		if( retval )
		{
			ps.flags |= CREATE; // set CREATE flag and try again
			errno = 0;
			retval = ioctl(fd, SETSINGLE, (unsigned long)&ps );
		}
		if( retval )
			printf("ioctl SETSINGLE in set_escapedlist returns %i ( %s )\n",-errno, strerror(errno));
	}

	close(fd);
	return 0;
}

int decodeData(char* inString, char* outStr)
{
	/* When decoding the Data we will do the following:
	   1.) Replace '+' with ' '
	   2.) Replace %xx to equivalent character
	*/
	int i = 0;
	int count = 0;
	char lbuf[3];

	while(inString[i])
	{
		switch (inString[i])
		{
				//if the character is a plus sign (+) then append a space
			case '+':
				outStr[count] = ' ';
				break;
			case '%':
				i++;
				lbuf[0] = inString[i];
				if (isupper(lbuf[0]))
					lbuf[0] = tolower(lbuf[0]);
				if ((lbuf[0] < '0' && lbuf[0] > '9') && ((lbuf[0] < 'a' && lbuf[0] > 'f')))
					return -1;
				i++;
				lbuf[1] = inString[i];
				if (isupper(lbuf[1]))
					lbuf[1] = tolower(lbuf[1]);
				if ((lbuf[1] < '0' && lbuf[1] > '9') && ((lbuf[1] < 'a' && lbuf[1] > 'f')))
					return -1;
#define atoh(c) ((c) - (((c) >= '0' && (c) <= '9') ? '0' : ('a'-10)))
				outStr[count] = (unsigned char)((atoh(lbuf[0]) << 4) + atoh(lbuf[1]));
				break;
			default:
				outStr[count] = inString[i];
				break;
		}
		i++;
		count++;
	}
	outStr[count] = 0;
	return count;
}

static int validate_name(char *str)
{
	char mychar;
	while ((mychar = *str++) != '\0') {
		if (!((mychar >= '0' && mychar <= '9') || 
			(mychar >= 'a' && mychar <= 'z') || 
			(mychar >= 'A' && mychar <= 'Z') || 
			(mychar == '-') || (mychar == '_') || 
			(mychar == '.') || (mychar == '(') || 
			(mychar == ')'))) {
			return -EINVAL;	/* names are limited to a specific character set [0-9 a-z A-Z -_.()] */
		}
	}
	return 0;
}

static int set_list(EspRequest *ep, int argc, char **argv)
{
	struct	rdb_session *s = NULL;
	int	retval;
	char	buffer[MY_MAX_LIST_SIZE+1];
	char	name[MAX_NAME_LENGTH+1];
	char	value[MY_MAX_LIST_SIZE+1];
	char	*str, *p_pos;
	int	flag;
	int	flags = 0;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		printf( "rdb_open failed. %i ( %s )\n", -errno, strerror(errno));
		return -1;
	}

	retval = rdb_lock(s, 0);
	if (retval) {
		printf("rdb_lock failed. %i (%s)\n", -errno, strerror(errno));
	}

	if( argc > 1 )
	{
		if(strncmp( argv[1], "-p", 2 )==0)
		{
			flags |= PERSIST;
		}
	}
	for (str = argv[0]; *str != '\0';)
	{
		flag = 0;
		for (p_pos = str; *p_pos != '\0' && *p_pos != '=' && *p_pos != '&'; p_pos++) {
		}
		if (*p_pos == '=') { //value is fllowed
			flag = 1;
		} else if (*p_pos == '&') { // another varialbe followed
			flag = 2;
		}
		*p_pos = '\0';
		STRNCPY(name, str, sizeof(name));
		retval = validate_name(name);
		if (retval) {
			rdb_unlock(s);
			printf("SETLIST name(%s) not valid %i\n",name, retval);
			rdb_close(&s);
			return retval;
		}
		value[0] = '\0';
		if (flag == 1) {
			/*  when '=' found, do not delete variable */
			p_pos++;
			for (str = p_pos; *p_pos != '\0' && *p_pos != '&'; p_pos++) {
			}
			if (*p_pos == '&') { // more value variable is defined.
				flag = 2;
				*p_pos = '\0';
			} else {
				flag = 0;
			}
			STRNCPY(buffer, str, sizeof(buffer));
			decodeData(buffer, value); /* , MY_MAX_LIST_SIZE); */
		}
		str = p_pos;
		if (flag)
			str++;
		retval = rdb_update_string(s, name, value, flags, DEFAULT_PERM);
		if( retval ) {
			printf("rdb_update_string in set_list returns %i ( %s )\n",-errno, strerror(errno));
		}
	}
	rdb_unlock(s);
	rdb_close(&s);
	return 0;
}

static int rdb_lock_esp(EspRequest *ep, int argc, char **argv)
{
	struct	rdb_session *s = NULL;
	int	retval;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		printf( "rdb_open failed. %i ( %s )\n", -errno, strerror(errno));
		return -1;
	}

	if(!rdb_lock(s, 0) )
	{
		espSetReturnString(ep, "OK" );
	}
	else
	{
		espSetReturnString(ep, "ERROR" );
	}

	rdb_close(&s);
	return 0;
}

static int rdb_unlock_esp(EspRequest *ep, int argc, char **argv)
{
	struct	rdb_session *s = NULL;
	int	retval;

	if (argc < 1) {
		espError(ep, "Bad args");
		return -1;
	}

	retval = rdb_open(NULL, &s);
	if (retval) {
		printf( "rdb_open failed. %i ( %s )\n", -errno, strerror(errno));
		return -1;
	}

	if(!rdb_unlock(s) )
	{
		espSetReturnString(ep, "OK" );
	}
	else
	{
		espSetReturnString(ep, "ERROR" );
	}

	rdb_close(&s);
	return 0;
}


static int esp_wait_for_chg(EspRequest *ep, int argc, char **argv)
{
	struct rdb_session *s;
	char* name;
	char value[MY_MAX_LIST_SIZE+1];
	const char* value2;
	int len = sizeof(value);
	int retval;

	int sleep_cnt;
	int i;


	// check invalidation of parameter
	if (argc < 3) {
		espError(ep, "Bad args");
		return -1;
	}

	// open rdb
	retval = rdb_open(NULL, &s);
	if (retval) {
		if (argc > 1)
			espSetReturnString(ep, argv[1] );
		else
			espSetReturnString(ep, "" );
		return 0;
	}

	// store parameters
	name=argv[0];
	value2=argv[1];
	sleep_cnt=atoi(argv[2]);

	// setup default return value
	strcpy(value,"N/A" );

	i=0;
	while(i++<sleep_cnt) {
		if(rdb_get(s, name, value, &len)!=0) {
			strcpy(value,"N/A" );
		}
		// bypass if value is not matching to the given value
		if(strcmp(value,value2))
			break;

		sleep(1);
	}
	rdb_close(&s);

	espSetReturnString(ep, value);

    return 0;
}

static int esp_sleep(EspRequest *ep, int argc, char **argv)
{
	unsigned long usec;
	char buf[16];
	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}
    	STRNCPY( buf, argv[0], sizeof(buf));
	espError(ep, buf );
	usec = atol(buf)*1000;
	usleep( usec );	
	espError(ep, "sleep OK");
	return 0;
}

static int get_pid(EspRequest *ep, int argc, char **argv)
{
  FILE* pFile;
  char respBuff[10];
  char msg[32];

  if (argc != 1) {
	espError(ep, "Bad args");
	return -1;
  }
  strcpy(msg, "pidof ");
  strncat(msg, argv[0],25);

  if( (pFile = popen(msg, "r") ) == 0)
  {
    espError(ep, "getPID failed to open pipe\n");
	espSetReturnString(ep, "0" );
    return 0;
  }
  fgets(respBuff, 10, pFile);
  pclose(pFile);
  if( atoi(respBuff)>0 )
  {
	if( *(respBuff+strlen(respBuff)-1)=='\n' )
		*(respBuff+strlen(respBuff)-1)=0;
	espSetReturnString(ep, respBuff );
  }
  else
	espSetReturnString(ep, "0" );
  return 0;
}

static int esp_syslog(EspRequest *ep, int argc, char **argv)
{
	int loglevel;
	const char* msg;

	// check validation of argument
	if(argc!=2) {
		return MPR_ERR_BAD_ARGS;
	}

	// get parameters
	loglevel=atoi(argv[0]);
	msg=argv[1];

	syslog(loglevel,msg);

	return 0;
}

static int exec_cmd(EspRequest *ep, int argc, char **argv)
{
	FILE* fp;
	const char* cmd;
	char line[1024];

	// check validation of argument
	if(argc!=1) {
		return MPR_ERR_BAD_ARGS;
	}

	// get command
	cmd=argv[0];

	// open the command
	fp=popen(cmd,"r");
	if(!fp) {
		return MPR_ERR_NOT_FOUND;
	}

	// print to the web page
	while( fgets(line,sizeof(line),fp) ) {
		espWriteString(ep,line);
	}

	fclose(fp);

	return 0;
}

/*
 * similar to exec_cmd
 * but it sets return string rather than prints to the web page
 */
static int exec_cmd_str(EspRequest *ep, int argc, char **argv)
{
    FILE* fp;
    const char* cmd;
    char value[MY_MAX_LIST_SIZE+1];
    int j;

    // check validation of argument
    if(argc != 1 || argv == NULL || argv[0] == NULL) {
        return MPR_ERR_BAD_ARGS;
    }

    // get command
    cmd = argv[0];

    // open the command
    fp = popen(cmd,"r");
    if( !fp ) {
        return MPR_ERR_NOT_FOUND;
    }

    // store the stdout from cmd to value
    j = fread(value, 1, MY_MAX_LIST_SIZE, fp);
    value[j] = '\0';

    fclose(fp);
    espSetReturnString(ep, value);
    return 0;
}

static int get_rand(EspRequest *ep, int argc, char **argv)
{
char value[64];
	sprintf( value, "%u", rand());
	espSetReturnString(ep, value );
	return 0;
}

/*
 * escape special characters by a given method.
 * Params:
 *   argv[0]: source string. compulsory
 *   argv[1]: encoding method. optional, default to "e"
 *            "e": html entity encoding in the form of &#dd;
 *            "a": ascii encoding in the form of \x??
 *   argv[2]: a string containing all special characters to be escaped.
 *            optional, default:
 *              " ' & < > for entity encoding; " ' \ for ascii encoding.
 */
#define ENTITY_ENCODING_LEN 5
#define ASCII_ENCODING_LEN 4
static int str_escape(EspRequest *ep, int argc, char **argv)
{
    char value[MY_MAX_LIST_SIZE+1];
    int i, j;
    int idx;
    char *src;
    char *esc_list;
    char *method;
    int char_enc_len;

    if (argc < 1 || argc > 3 || !argv || !argv[0]) {
        espError(ep, "Bad args");
        return -1;
    }

    src = argv[0]; /* source string */
    if (argc >= 2) {
        if (!argv[1]) {
            espError(ep, "Bad args");
            return -1;
        }
        method = argv[1]; /* specified encoding method */
    } else {
        method = "e"; /* default encoding method */
    }
    if (argc == 3) {
        if (!argv[2]) {
            espError(ep, "Bad args");
            return -1;
        }
        esc_list = argv[2]; /* specified special characters */
    } else {
        /* default special characters */
        esc_list = (method[0] == 'e' ? "\"\'&<>" : "\"\'\\");
    }

    /* length of an encoded special character */
    char_enc_len = (method[0]=='e' ? ENTITY_ENCODING_LEN : ASCII_ENCODING_LEN);
    for (i = j = 0; j < MY_MAX_LIST_SIZE && src[i]; i++) {
        for (idx = 0; idx < strlen(esc_list); idx++) {
            if (src[i] == esc_list[idx]) {
                break;
            }
        }
        if (idx < strlen(esc_list)) {
            // special character found, convert it
            if (j + char_enc_len > MY_MAX_LIST_SIZE) {
                // not enough space for converted string
                break;
            }
            sprintf(value+j, (method[0] == 'e' ? "&#%02d;" : "\\x%02x"), src[i]);
            j += char_enc_len;
        } else {
            // not a character entity, pass through
            value[j++] = src[i];
        }
    }
    value[j] = 0;

    espSetReturnString(ep, value);

    return 0;
}

/*
 * This implements <% random_token = generate_random_token("prefix string"); %> command.
 * It generates base64-encoded(BLAKE2s-hash-ed(prefix string+random number)).
 * '+', '/', and '=' in base64-encoded string are converted to 'n', 't', and 'c' respectively.
 * espError "Bad args" for invalid arguments or failure in allocating memory or on error
 * espSetReturnString oputput string
 */
static int generate_random_token(EspRequest *ep, int argc, char **argv)
{
	blake2s_state S[1];
	char out[BLAKE2S_OUTBYTES] = {0};
	static int init = 0;
	int random_num, buffer_length, prefix_len;
	char *buffer = NULL;
	char *base64_out = NULL;
	int ret = 0;

	int len;

	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}

	if (!init) {
		srand(time(NULL));
		init = 1;
	}
	random_num = rand();
	prefix_len = strlen(argv[0]);
	buffer_length = prefix_len + sizeof(random_num);
	if (!(buffer = malloc(buffer_length)) || !(base64_out = malloc(cdcs_base64_get_encoded_len(BLAKE2S_OUTBYTES)))) {
		espError(ep, "Low memory");
		ret = -1;
	}
	else {
		memcpy(buffer, argv[0], prefix_len);
		memcpy(buffer + prefix_len, &random_num, sizeof(random_num));

		/* blake2s-hash */
		blake2s_init( S, BLAKE2S_OUTBYTES );
		blake2s_update( S, buffer, buffer_length );
		blake2s_final( S, out, BLAKE2S_OUTBYTES );

		/* base64-encode */
		if (cdcs_base64encode(out, BLAKE2S_OUTBYTES, base64_out, 0) > 0) {
			/* convert special characters to alphabet */
			char *c;
			for (c = base64_out; *c != 0; c++) {
				if (*c == '+') {
					*c = 'n';
				}
				else if (*c == '/') {
					*c = 't';
				}
				else if (*c == '=') {
					*c = 'c';
				}
			}
			/* base64_out will be strdup-ed here */
			espSetReturnString(ep, base64_out);
		}
		else {
			espError(ep, "Error");
			ret = -1;
		}
	}

	free(base64_out);
	free(buffer);

	return ret;
}

/*
 * This implements <% validated number string = validate_number("string"); %> command.
 * It checks whether the input string can be converted to a number.
 * espError "Bad args" for invalid arguments or failure in converting
 * espSetReturnString converted string
 */
static int validate_number(EspRequest *ep, int argc, char **argv)
{
	char *endptr;
	long int val;
	char out[32];

	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}

	val = strtol(argv[0], &endptr, 10);
	if (*endptr == '\0'){
		sprintf(out, "%ld", val);
		espSetReturnString(ep, out);
		return 0;
	}
	else {
		espError(ep, "Bad arg");
		return -1;
	}
}

/*
 * This implements <% number = string_to_number("string"); %> command.
 * It converts the input string to number.
 * espError "Bad args" for invalid arguments or failure in converting
 * espSetReturn the converted number
 */
static int string_to_number(EspRequest *ep, int argc, char **argv)
{
	char *endptr;
	long int val;

	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}

	val = strtol(argv[0], &endptr, 10);
	if (*endptr == '\0'){
		if (sizeof(val) == sizeof(int)) {
			espSetReturn(ep, mprCreateIntegerVar(val));
			return 0;
		}
		else if (sizeof(val) == sizeof(int64)) {
			espSetReturn(ep, mprCreateInteger64Var(val));
			return 0;
		}
		else {
			espError(ep, "Data type is not supported");
			return -1;
		}
	}
	else {
		espError(ep, "Bad arg");
		return -1;
	}
}

/*
 * This implements <% validated trimmed string = trim_string("string to be trimmed", "part to search for"); %> command.
 * It searches for a part in input string. If found, it trims from that part to the end of the input string.
 * espError "Bad args" for invalid arguments
 * espSetReturnString trimmed string
 */
static int trim_string(EspRequest *ep, int argc, char **argv)
{
	char *p;

	if (argc != 2) {
		espError(ep, "Bad args");
		return -1;
	}

	if (p = strstr(argv[0], argv[1])) {
		*p = 0;
	}

	espSetReturnString(ep, argv[0]);
	return 0;
}

/*
 * This implements <% len = string_length("input string"); %> command.
 * esperror "Bad args" for invalid arguments
 * espSetReturn the length of string as an integer
 */
static int string_length(EspRequest *ep, int argc, char **argv)
{
	size_t len;

	if (argc != 1) {
		espError(ep, "Bad args");
		return -1;
	}

	len = strlen(argv[0]);

	espSetReturn(ep, mprCreateIntegerVar(len));
	return 0;
}

/*
 * This implements <% matched = string_regex_match("input string", "regular expression pattern"); %> command.
 * esperror "Bad args" for invalid arguments
 * espSetReturn bool true if matched, or false unmatched or error
 */
static int string_regex_match(EspRequest *ep, int argc, char **argv)
{
	regex_t regex;
	int ret;

	if (argc != 2) {
		espError(ep, "Bad args");
		return -1;
	}

	ret = regcomp(&regex, argv[1], REG_EXTENDED);
	if (ret) {
		espError(ep, "Could not compile regex");
		return -1;
	}

	ret = regexec(&regex, argv[0], 0, NULL, 0);
	if (!ret) {
		espSetReturn(ep, mprCreateBoolVar(1));
	}
	else if (ret == REG_NOMATCH) {
		espSetReturn(ep, mprCreateBoolVar(0));
		ret = 0;
	}
	else {
		espError(ep, "Regex match failed");
	}

	regfree(&regex);

	return ret;
}

/******************************************************************************/

void espRegisterProcs()
{
	espDefineStringCFunction(0, "rename", renameProc, 0);
	espDefineStringCFunction(0, "write", writeProc, 0);
	espDefineStringCFunction(0, "setHeader", setHeaderProc, 0);
	espDefineStringCFunction(0, "redirect", redirectProc, 0);
	espDefineStringCFunction(0, "include", includeProc, 0);

	espDefineStringCFunction(0, "rdb_exists",rdb_exists, 0);
	espDefineStringCFunction(0, "get_single", get_single, 0);
	espDefineStringCFunction(0, "get_single_direct", get_single_direct, 0);
	espDefineStringCFunction(0, "set_single", set_single, 0);
	espDefineStringCFunction(0, "set_single_direct", set_single_direct, 0);
	espDefineStringCFunction(0, "get_list", get_list, 0);
	espDefineStringCFunction(0, "set_list", set_list, 0);
	espDefineStringCFunction(0, "unescape_string", unescape_string, 0);
	espDefineStringCFunction(0, "set_escapedlist", set_escapedlist, 0);
	espDefineStringCFunction(0, "rdb_lock", rdb_lock_esp, 0);
	espDefineStringCFunction(0, "rdb_unlock", rdb_unlock_esp, 0);
	espDefineStringCFunction(0, "esp_sleep", esp_sleep, 0);
	espDefineStringCFunction(0, "get_pid", get_pid, 0);
	espDefineStringCFunction(0, "get_rand", get_rand, 0);

	espDefineStringCFunction(0, "esp_wait_for_chg", esp_wait_for_chg, 0);

	espDefineStringCFunction(0, "exec_cmd", exec_cmd, 0);
	espDefineStringCFunction(0, "exec_cmd_str", exec_cmd_str, 0);

	// use this esp function instead of trace - builtin trace function is limited by appweb loglevel
	espDefineStringCFunction(0, "syslog", esp_syslog, 0);

	espDefineStringCFunction(0, "str_escape", str_escape, 0);
	espDefineStringCFunction(0, "base64_encode", base64_encode, 0);
	espDefineStringCFunction(0, "generate_random_token", generate_random_token, 0);
	espDefineStringCFunction(0, "validate_number", validate_number, 0);
	espDefineStringCFunction(0, "string_to_number", string_to_number, 0);
	espDefineStringCFunction(0, "trim_string", trim_string, 0);
	espDefineStringCFunction(0, "string_length", string_length, 0);
	espDefineStringCFunction(0, "string_regex_match", string_regex_match, 0);

#if BLD_FEATURE_SESSION
	/*
	 *	Create and use are synonomous
	 */
	espDefineStringCFunction(0, "useSession", useSessionProc, 0);
	espDefineStringCFunction(0, "createSession", useSessionProc, 0);
	espDefineStringCFunction(0, "destroySession", destroySessionProc, 0);
#endif
}

/******************************************************************************/

#else
void mprEspControlsDummy() {}

#endif /* BLD_FEATURE_ESP_MODULE */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: sw=4 ts=4 
 */

