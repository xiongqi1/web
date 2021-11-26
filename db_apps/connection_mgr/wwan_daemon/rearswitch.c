#include "rearswitch.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h> 
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <libgpio.h>

////////////////////////////////////////////////////////////////////////
void rearswitch_destroy(rearswitch* pR)
{
	if(!pR)
		return;

	gpio_exit();

	free(pR);
}
////////////////////////////////////////////////////////////////////////
int rearswitch_readPin(rearswitch* pR)
{
	return gpio_read(pR->iPin)!=0;
}
////////////////////////////////////////////////////////////////////////
int rearswitch_setDir(rearswitch* pR, int fOut)
{
	int rval;

	if (fOut)
		rval = gpio_set_output(pR->iPin,gpio_read(pR->iPin));
	else
		rval = gpio_set_input(pR->iPin);

	return rval;
}
////////////////////////////////////////////////////////////////////////
rearswitch* rearswitch_create(int iPin)
{
	int rval;
	rearswitch* pR;

	rval = gpio_request_pin(iPin);
	if (rval < 0) {
		perror("Couldn't request rearswitch GPIO");
		return NULL;
	}

	pR=malloc(sizeof(rearswitch));
	if(!pR)
		goto error;

	memset(pR,0,sizeof(*pR));

	pR->iPin=iPin;
	
	return pR;

error:
	return NULL;
}
////////////////////////////////////////////////////////////////////////
rearswitch* rearswitch_create_readpin(int iPin)
{
	// allocate pin
	rearswitch* pS=0;
	

	// bypass if negitvie pin
	if(iPin<0)
		goto error;

	// create pin
	pS=rearswitch_create(iPin);
	if(!pS)
	{
		fprintf(stderr,"failed to allocate memory for rear switch - pin=%d\n",iPin);
		goto error;
	}

	// set direction
	if(rearswitch_setDir(pS,0))
	{
		fprintf(stderr,"failed to set direction of pin - %d\n",iPin);
		goto error;
	}	

	return pS;

error:
	rearswitch_destroy(pS);
	return 0;
}
