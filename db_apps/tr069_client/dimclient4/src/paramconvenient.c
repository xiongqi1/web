/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** Implement the access to some often needed parameters
*/

#include "paramconvenient.h"
#include "parameter.h"
#include "globals.h"
#include "eventcode.h"

char *server_url = NULL;

struct ConfigManagement conf;

static char *server_username = NULL;
static char *server_passwd = NULL;
static struct DeviceId devId;
static char server_addr[32];
static time_t sessionId = (time_t)0;
static	char	url[256] = "";

static bool isSessionNew( void );
static void updateServerAddr( void );

/**	Returns	the	PeriodicInformInterval Value
 *  If PeriodicInformEnable == false return 0
 */
unsigned int
getPeriodicInterval( void )
{
	bool *periodicEnabled;
	unsigned int *periodicInterval = NULL;

	if ( getParameter(PERIODIC_INFORM_ENABLE, &periodicEnabled) != OK)
		return 0;
	if ( *periodicEnabled == false )
		return 0;
	if ( *periodicEnabled == true
	    && getParameter (PERIODIC_INFORM_INTERVAL, &periodicInterval) == OK)
		return *periodicInterval;

	return 0;
}

/**	Returns	the	PeriodicInformTime Value
 *  If PeriodicInformEnable == false return 0
 */
unsigned int
getPeriodicTime( void )
{
	bool *periodicEnabled;
	unsigned int *periodicInterval = NULL;

	if ( getParameter(PERIODIC_INFORM_ENABLE, &periodicEnabled) != OK)
		return 0;
	if ( *periodicEnabled == false )
		return 0;
	if ( *periodicEnabled == true
	    && getParameter (PERIODIC_INFORM_TIME, &periodicInterval) == OK)
		return *periodicInterval;

	return 0;
}

/**
 * Get the ACS connection URL
 */
const char *
getServerURL( void )
{
  char *tmp;
  int   ret = 0;

  // check if the url is not initialized yet or the session is new
  if ( server_url == NULL || isSessionNew() ) {
    ret = getParameter (MANAGEMENT_SERVER_URL, &tmp);
    if ( ret != OK) {
    	DEBUG_OUTPUT (
    			dbglog (SVR_ERROR, DBG_PARAMETER, "getServerURL: (%s) failed ret = %d\n", MANAGEMENT_SERVER_URL, ret);
    	)

      server_url = NULL;
    } else {
      server_url = strnDupSession(server_url, tmp, strlen(tmp));
      updateServerAddr();
      if (*url && strcmp(url, server_url))
    	  addEventCodeSingle( EV_BOOTSTRAP );
      strcpy (url, server_url);
    }
    sessionId = getSessionId();
  }

  DEBUG_OUTPUT (
		  dbglog (SVR_DEBUG, DBG_PARAMETER,
				  "getServerURL: server_url = \"%s\" [0x%hX] server_url = %p ret = %d\n",
				  server_url, (unsigned int) server_url[0], server_url, ret);
  )

  return server_url;
}

#ifdef WITH_STUN_CLIENT

/*
 * Return IPv4 address found for the ServerURL
 */
const char *
getServerAddr( void )
{
  return server_addr;
}

#endif /* WITH_STUN_CLIENT */

/* 
 * When server_url is updated, also update server_addr
 */
static void
updateServerAddr (void)
{
  strcpy( server_addr, "");

  /* Parse URL into extra host and port parts */
  char *to, *from, tmp_buf[64];
  int count;
  to = tmp_buf;
  count = 1;

  if (strncasecmp(server_url,"http://",7) == 0) {
    from = server_url + 7;
    while ((*from != ':' ) && (count<sizeof(tmp_buf))) {
      *to = *from;
      to++;
      from++;
      count++;
      if (count >= sizeof(tmp_buf))
        break;
    }
    *to = '\0';

    struct hostent     *he;
    struct sockaddr_in server;

    if((he = gethostbyname( tmp_buf ))) {
        bzero((char *) &server, sizeof(server));
        server.sin_family = AF_INET;
        memcpy((char *)&server.sin_addr, (char *) he->h_addr_list[0], he->h_length);
        sprintf (server_addr, "%s",inet_ntoa( server.sin_addr));
	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_PARAMETER, "updateServerAddr() = %s\n",server_addr);
	)
    } else {
	DEBUG_OUTPUT (
			dbglog (SVR_DEBUG, DBG_PARAMETER, "updateServerAddr(): DNS gethostbyname() failure\n");
	)
    }
  }
}

/*
 *	Returns	the	Username defined in	
 *      *.ManagementServer.Username
 *	if the parameter doesn't exist or is empty return NULL
 */
const char *
getUsername( void )
{
  int ret = OK;
  char *oui;
  char *serial;
  char *tmp;
	
//  if ( server_username == NULL || isSessionNew() ) {
  isSessionNew();

    if (getParameter (MANAGEMENT_SERVER_USERNAME, &tmp) == OK)	{
      sessionId = getSessionId();
      if ( tmp != NULL ) 
    	  server_username = strnDupSession(server_username, tmp, strlen(tmp));
      else
    	  server_username = tmp;
      
      if (server_username == NULL || strlen (server_username) == 0) {
    	  ret = getParameter (DEVICE_INFO_MANUFACTURER_OUI, &oui);
    	  ret += getParameter (DEVICE_INFO_SERIAL_NUMBER, &serial);

    	  if (ret == OK) {
    		  server_username = (char *) emallocSession (strlen (oui) + strlen (serial) + 3);
    		  strcpy (server_username, oui);
    		  strcat (server_username, "-");
    		  strcat (server_username, serial);
	
    		  /* set the parameter for later usage  */
    		  setParameter (MANAGEMENT_SERVER_USERNAME, server_username);

    		  /* reread the parameter to get the pointer to the new parameter,
    		   * the old username  pointer is freed after the mainloop exits
    		   */
    		  getParameter (MANAGEMENT_SERVER_USERNAME, &tmp);
    		  server_username = strnDupSession(server_username, tmp, strlen(tmp));
    	  } else
    		  server_username = NULL;
      }
    }
//  }

  return server_username;
}

/**	Returns	the	password defined in	
	InternetGatewayDevice.ManagementServer.Password
	if the parameter doesn't exist or is empty return NULL
*/
const char *
getPassword( void )
{
  int ret = OK;
  char *oui;
  char *serial;
  char *tmp;

//  if ( server_passwd == NULL || isSessionNew() ) {
  isSessionNew();

    if (getParameter (MANAGEMENT_SERVER_PASSWORD, &tmp) == OK) {
      if ( tmp != NULL ) 
    	  server_passwd = strnDupSession(server_passwd, tmp, strlen(tmp));
      else
    	  server_passwd = tmp;

      sessionId = getSessionId();
      if (server_passwd == NULL || strlen (server_passwd) == 0) {
    	  ret = getParameter (DEVICE_INFO_MANUFACTURER_OUI, &oui);
    	  ret += getParameter (DEVICE_INFO_SERIAL_NUMBER, &serial);

    	  if (ret == OK) {
    		  server_passwd = (char *) emallocSession(strlen (oui) + strlen (serial) + 3);
    		  strcpy (server_passwd, serial);
    		  strcat (server_passwd, "-");
    		  strcat (server_passwd, oui);

    		  /* set the parameter for later usage
    		   */
    		  setParameter (MANAGEMENT_SERVER_PASSWORD, server_passwd);

    		  /* reread the parameter to get the pointer to the new parameter,
    		   * the old passwd pointer is freed after the mainloop exits
    		   */
    		  getParameter (MANAGEMENT_SERVER_PASSWORD, &tmp);
    		  server_passwd = strnDupSession(server_passwd, tmp, strlen(tmp));
    	  } else
    		  server_passwd = NULL;
      }
    }
//  }

    return server_passwd;
}

/**
 * Last modification: San, 15 July 2011
 * Return the connection URL in parameter connURL, with which the ACS connects to the CPE,
 * when the ACS sends a ConnectionRequest
 *  Return OK - if result of function is OK
 */
int getConnectionURL (void * connURL)
{
	int ret;
	if ((ret = getParameter (CONNECTION_REQUEST_URL, connURL)) != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_REQUEST, "getConnectionURL()->getParameter(%s) returns error = %i\n",CONNECTION_REQUEST_URL,ret);
		)
		return ret;
	}
	return OK;
}

/**
 * Last modification: San, 15 July 2011
 * Return the Username in parameter connUser, with which the ACS connects to the CPE,
 * when the ACS sends a ConnectionRequest
 *  Return OK - if result of function is OK
 */
int getConnectionUsername (void * connUser)
{
	int ret;
	if ((ret = getParameter (CONNECTION_REQUEST_USERNAME, connUser)) != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_REQUEST, "getConnectionUsername()->getParameter(%s) returns error = %i\n",CONNECTION_REQUEST_USERNAME,ret);
		)
		return ret;
	}
	return OK;
}

/**
 * Last modification: San, 15 July 2011
 * Return the Password in parameter connPass, with which the ACS connects to the CPE,
 * when the ACS sends a ConnectionRequest
 *  Return OK - if result of function is OK
 */
int getConnectionPassword (void * connPass)
{
	int ret;
	if ((ret = getParameter (CONNECTION_REQUEST_PASSWORD, connPass)) != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_REQUEST, "getConnectionPassword()->getParameter(%s) returns error = %i\n",CONNECTION_REQUEST_PASSWORD,ret);
		)
		return ret;
	}
	return OK;
}

/** Return the DeviceId structure, which is used in the inform message
 */
struct DeviceId *
getDeviceId (void)
{
	getParameter (DEVICE_INFO_MANUFACTURER, &devId.Manufacturer);
	getParameter (DEVICE_INFO_MANUFACTURER_OUI, &devId.OUI);
	getParameter (DEVICE_INFO_PRODUCT_CLASS, &devId.ProductClass);
	getParameter (DEVICE_INFO_SERIAL_NUMBER, &devId.SerialNumber);
	return &devId;
}

static bool
isSessionNew(void) 
{
	if ( isNewSession(sessionId) ) {
		server_passwd = NULL;
		server_username = NULL;
		sessionId = getSessionId();
		return true;
	} else 
		return false;
}

/**
 * Init configuration struct with a data from the file, which a path is a function parameter
 */
int initConfStruct(const char *confPath )
{

	memset(&conf.url, 0, sizeof conf.url);
	memset(&conf.username, 0, sizeof conf.username);
	memset(&conf.password, 0, sizeof conf.password);
	memset(&conf.notificationTime, 0, sizeof conf.notificationTime);

#if 0 //don't need this at this time
	char line[MAX_CONF_FILE_SRT_LEN];

	int ret;
	FILE* fp = fopen(confPath,"r");
	if (!fp)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "initConfStruct: fopen() error. Check if path set correctly in the start_cwmp.sh\n");
		)
		return ERR_ACCESS_FILE;
	}

	char * point1;
	char paramName[20] = {'\0'};
	int len = 0;

	while (fgets(line, sizeof(line), fp))
	{
		memset(&paramName, 0, sizeof paramName);
		point1 = strchr(line,0x3D); // symbol '='
		if (!point1) { continue; }
		len = strlen(line) - strlen(point1);
		if (len < 1) { continue; }
		strncpy(paramName,line,len);

		//printf("paramName = %s\n", paramName);

		if (0 == strcmp(paramName,"ManagementServerURL")){
			sscanf(line, "ManagementServerURL=%s", &conf.url);
			if (strlen(conf.url) > 0) {
				ParameterValue temp_value;
				temp_value.in_cval = conf.url;
				ret = storeParamValue(MANAGEMENT_SERVER_URL, StringType, &temp_value);
				if (ret != OK)
				{
					DEBUG_OUTPUT (
							dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "initConfStruct()->storeParamValue(%s) returns error = %i\n",MANAGEMENT_SERVER_URL,ret);
					)
					fclose(fp);
					return ret;
				}
			}
			continue;
		}

		if (0 == strcmp(paramName,"Username")){
			sscanf(line, "Username=%s", &conf.username);
			if (strlen(conf.username) > 0) {
				ParameterValue temp_value2;
				temp_value2.in_cval = conf.username;
				ret = storeParamValue(MANAGEMENT_SERVER_USERNAME, StringType, &temp_value2);
				if (ret != OK)
				{
					DEBUG_OUTPUT (
							dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "initConfStruct()->storeParamValue(%s) returns error = %i\n",MANAGEMENT_SERVER_USERNAME,ret);
					)
					fclose(fp);
					return ret;
				}
			}
			continue;
		}


		if (0 == strcmp(paramName,"Password")){
			sscanf(line, "Password=%s", &conf.password);
			if (strlen(conf.password) > 0) {
				ParameterValue temp_value3;
				temp_value3.in_cval = conf.password;
				ret = storeParamValue(MANAGEMENT_SERVER_PASSWORD, StringType, &temp_value3);
				if (ret != OK)
				{
					DEBUG_OUTPUT (
							dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "initConfStruct()->storeParamValue(%s) returns error = %i\n",MANAGEMENT_SERVER_PASSWORD,ret);
					)
					fclose(fp);
					return ret;
				}
			}
			continue;
		}


		if (0 == strcmp(paramName,"NotificationTime")){
			unsigned int inttmp;
			int res = sscanf(line,"NotificationTime=%d", &inttmp);
			if (res <= 0 ){
				conf.notificationTime = 0; // Default
			}
			else {
				conf.notificationTime = inttmp;
			}
			continue;
		}
	}

	fclose(fp);

	/*
	printf("conf.url = %s\n", conf.url);
	printf("conf.username = %s\n", conf.username);
	printf("conf.password = %s\n", conf.password);
	printf("conf.notificationTime = %d\n", conf.notificationTime);
	*/
#endif
//stupid but working
	conf.notificationTime = 60;
 
	return OK;
}

/*
 *	Returns	the	URL defined in	configuration file
 *	if the parameter doesn't exist or is empty return NULL
 */
char * getServerURLFromConf( void )
{
	return conf.url;
}

/*
 *	Returns	the	Username defined in	configuration file
 *	if the parameter doesn't exist or is empty return NULL
 */
char * getUsernameFromConf( void )
{
	return conf.username;
}


/*
 *	Returns	the	Password defined in	configuration file
 *	if the parameter doesn't exist or is empty return NULL
 */
char * getPasswordFromConf( void )
{
	return conf.password;
}



