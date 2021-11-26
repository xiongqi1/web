#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h> 
#include <sys/times.h> 

#include "led.h"
#include "stridx.h"
#include "rdb.h"

#define LED_ENTRY_TIMEOUT	3

const char* led_sys_dir="/sys/class/leds";

static int led_put_int(const char* led,int led_idx,const char* entry,int val,int timeout);
static int led_put_str(const char* led,int led_idx,const char* entry,const char* val,int timeout);


int led_put_2leds_value(const char* led1,const char* led2,const char* entry,const char* val_str,int val_int,int sync)
{
	char fname[PATH_MAX];
	char line[1024];
	int len;
	int written;
	
	int fd1;
	int fd2;

	const char* crlf="\n";
	
	fd2=fd1=-1;
	
	// open led1 entry
	snprintf(fname,sizeof(fname),"%s/%s/%s",led_sys_dir,led1,entry);
	if((fd1=open(fname,O_WRONLY))<0) {
		syslog(LOG_ERR,"#%d failed to open led1 (%s) - %s",__LINE__,fname,strerror(errno));
	}
	
	snprintf(fname,sizeof(fname),"%s/%s/%s",led_sys_dir,led2,entry);
	if((fd2=open(fname,O_WRONLY))<0) {
		syslog(LOG_ERR,"#%d failed to open led1 (%s) - %s",__LINE__,fname,strerror(errno));
	}
	
	// build value
	if(val_str)
		len=snprintf(line,sizeof(line),sync?"%s":"%s\n",val_str);
	else
		len=snprintf(line,sizeof(line),sync?"%d":"%d\n",val_int);
		
	syslog(LOG_DEBUG,"[LED] setting led1(%s) - entry=%s,val=%s,len=%d",led1,entry,line,len);
	syslog(LOG_DEBUG,"[LED] setting led2(%s) - entry=%s,val=%s,len=%d",led2,entry,line,len);
	
	// write
	if( (written=write(fd1,line,len))<0 ) {
		syslog(LOG_ERR,"#%d failed to write to led1 (fname=%s,len=%d) - %s",__LINE__,fname,len,strerror(errno));
	}
	
	// write
	if( (written=write(fd2,line,len))<0 ) {
		syslog(LOG_ERR,"#%d failed to write to led2 (fname=%s,len=%d) - %s",__LINE__,fname,len,strerror(errno));
	}
	

	/* sync write */
	if(sync) {
		sched_yield();
	
		/* sync write */
		len=strlen(crlf);
		if(fd1>=0)
			write(fd1,crlf,len);
		if(fd2>=0)
			write(fd2,crlf,len);
	}

	if(fd1>=0)
		close(fd1);
		
	if(fd2>=0)
		close(fd2);
	
	return 0;
}


int led_wait_entry(const char* led,const char* entry,int timeout)
{
	char fname[PATH_MAX];
	int fd;
	
	clock_t s;
	clock_t c;
	
	struct tms tmsbuf;
	
	fd=-1;
	
	/* get start time */
	s=c=times(&tmsbuf);
	
	/* get entry name */
	snprintf(fname,sizeof(fname),"%s/%s/%s",led_sys_dir,led,entry);
	
	/* wait until open */
	while(c-s<timeout) {
		
		/* bypass if opened */
		if((fd=open(fname,O_WRONLY))>=0)
			break;
			
		/* schedule */
		sched_yield();
		
		/* get current time */
		c=times(&tmsbuf);
	}
	
	if(fd>=0)
		close(fd);
		
	return fd;
}

int led_put_value(const char* led,const char* entry,const char* val_str,int val_int)
{
	char fname[PATH_MAX];
	char line[1024];
	int fd;
	int len;
	int written;
	
	fd=-1;
	
	// open led entry
	snprintf(fname,sizeof(fname),"%s/%s/%s",led_sys_dir,led,entry);
	if((fd=open(fname,O_WRONLY))<0) {
		syslog(LOG_ERR,"#%d failed to open led (%s) - %s",__LINE__,fname,strerror(errno));
		goto err;
	}
	
	// build value
	if(val_str)
		len=snprintf(line,sizeof(line),"%s\n",val_str);
	else
		len=snprintf(line,sizeof(line),"%d\n",val_int);
	
	syslog(LOG_DEBUG,"[LED] setting led(%s) - entry=%s,val=%s",led,entry,line);
	
	// write
	if( (written=write(fd,line,len))<0 ) {
		syslog(LOG_ERR,"#%d failed to write to led (%s) - %s",__LINE__,fname,strerror(errno));
		goto err;
	}
		
	close(fd);
	
	return 0;
err:
	if(fd>=0)
		close(fd);
		
	return -1;
		
}

static int led_put_int(const char* led,int led_idx,const char* entry,int val,int timeout)
{
	char ledname[PATH_MAX];
	
	snprintf(ledname,sizeof(ledname),led,led_idx);
	
	/* wait if timeout is set */
	if(timeout)
		led_wait_entry(ledname,entry,timeout);
	
	return led_put_value(ledname,entry,NULL,val);
}

static int led_put_str(const char* led,int led_idx,const char* entry,const char* val,int timeout)
{
	char ledname[PATH_MAX];
	
	snprintf(ledname,sizeof(ledname),led,led_idx);
	
	/* wait if timeout is set */
	if(timeout)
		led_wait_entry(ledname,entry,timeout);
	
	return led_put_value(ledname,entry,val,0);
}

static int is_led_held(const char* led,int led_idx)
{
	char ledname[PATH_MAX];
	char ledrdb[PATH_MAX];
	const char* p;
	
	snprintf(ledname,sizeof(ledname),led,led_idx);
	snprintf(ledrdb,sizeof(ledrdb),"dispd.usrled.%s",ledname);
	
	if((p=rdb_getVal(ledrdb))==NULL)
		return 0;
	
	return atoi(p);
}

void led_set_solid(const char* led,int led_idx,int on)
{
	// bypass if led is held
	if(is_led_held(led,led_idx))
		return;
	
	led_put_str(led,led_idx,LED_ENTRY_TRIGGER,"none",0);
	led_put_int(led,led_idx,LED_ENTRY_BRIGHTNESS,on,LED_ENTRY_TIMEOUT);
}

int led_set_array(const char* leds[],const char* entry,const char* str,const char** strs,const int* vals)
{
	int i;
	int led_cnt;
	
	char fname[PATH_MAX];
	char line[1024];
	int len;
	int written;
	
	int fds[1024];

	int half;
	int idx;
	int j;

	// get total number of leds
	led_cnt=0;
	while(leds[led_cnt]!=NULL) 
		led_cnt++;
		
	// set fds
	memset(fds,0xff,sizeof(fds));
	
	half=led_cnt/2;

	/* write */
	for(i=0;i<half;i++) {
		for(j=0;j<2;j++) {
			idx=i+(j*half);

			// open led
			snprintf(fname,sizeof(fname),"%s/%s/%s",led_sys_dir,leds[idx],entry);
			if((fds[idx]=open(fname,O_WRONLY))<0) {
				syslog(LOG_ERR,"#%d failed to open led1 (%s) - %s",__LINE__,fname,strerror(errno));
			}
			
			if(str)
				len=snprintf(line,sizeof(line),"%s",str);
			else if(strs)
				len=snprintf(line,sizeof(line),"%s",strs[idx]);
			else
				len=snprintf(line,sizeof(line),"%d",vals[idx]);
	
			syslog(LOG_DEBUG,"[LED] setting array led(%s) - entry=%s,val=%s",leds[idx],entry,line);
			
			// write
			if( (written=write(fds[idx],line,len))<0 ) {
				syslog(LOG_ERR,"#%d failed to write to led (fname=%s,len=%d) - %s",__LINE__,fname,len,strerror(errno));
			}
		}
	}
	
	if(led_cnt%2) {
		idx=led_cnt-1;

		// open led
		snprintf(fname,sizeof(fname),"%s/%s/%s",led_sys_dir,leds[idx],entry);
		if((fds[idx]=open(fname,O_WRONLY))<0) {
			syslog(LOG_ERR,"#%d failed to open led1 (%s) - %s",__LINE__,fname,strerror(errno));
		}
		
		if(str)
			len=snprintf(line,sizeof(line),"%s",str);
		else if(strs)
			len=snprintf(line,sizeof(line),"%s",strs[idx]);
		else
			len=snprintf(line,sizeof(line),"%d",vals[idx]);

		syslog(LOG_DEBUG,"[LED] setting array led(%s) - entry=%s,val=%s",leds[idx],entry,line);
		
		// write
		if( (written=write(fds[idx],line,len))<0 ) {
			syslog(LOG_ERR,"#%d failed to write to led (fname=%s,len=%d) - %s",__LINE__,fname,len,strerror(errno));
		}
	}
	
	
	/* close leds */
	for(i=0;i<half;i++) {
		if(fds[i+(0*half)]>=0)
			close(fds[i+(0*half)]);
		if(fds[i+(1*half)]>=0)
			close(fds[i+(1*half)]);
	}
	if(led_cnt%2) {
		if(fds[led_cnt-1]>=0)
			close(fds[led_cnt-1]);
	}
	
	return 0;
}

void led_set_2timers(const char* led,const char* led2,int on, int off)
{
	// bypass if led is held
	if(is_led_held(led,0) || is_led_held(led2,0))
		return;
	
	// TODO: we may need kernel level led timer sync
	
	/* set timer triggers */
	led_put_2leds_value(led,led2,LED_ENTRY_TRIGGER,"timer",-1,0);
	
	/* quickly turn on two LEDs */
	led_put_2leds_value(led,led2,LED_ENTRY_BRIGHTNESS,NULL,255,0);
	
	/* disable on-delays - stop kernel timer */
	led_put_int(led,-1,LED_ENTRY_DELAY_ON,0,LED_ENTRY_TIMEOUT);
	led_put_int(led2,-1,LED_ENTRY_DELAY_ON,0,LED_ENTRY_TIMEOUT);
	
	/* turn on two LEDs again (to avoid a race condition - any previous timer could turn off) */
	led_put_2leds_value(led,led2,LED_ENTRY_BRIGHTNESS,NULL,255,0);
	
	/* set off-delays */
	led_put_int(led,-1,LED_ENTRY_DELAY_OFF,off,LED_ENTRY_TIMEOUT);
	led_put_int(led2,-1,LED_ENTRY_DELAY_OFF,off,LED_ENTRY_TIMEOUT);
	
	/* set on delay - start kernel timers */
	led_put_2leds_value(led,led2,LED_ENTRY_DELAY_ON,NULL,on,1);

	/* set timer triggers */
	led_put_2leds_value(led,led2,LED_ENTRY_TRIGGER,"timer",-1,0);
}
		
void led_set_timer(const char* led,int led_idx,int on, int off)
{
	// bypass if led is held
	if(is_led_held(led,led_idx))
		return;
	
	led_put_str(led,led_idx,LED_ENTRY_TRIGGER,"timer",0);
	led_put_int(led,led_idx,LED_ENTRY_DELAY_ON,on,LED_ENTRY_TIMEOUT);
	led_put_int(led,led_idx,LED_ENTRY_DELAY_OFF,off,0);
}
		  
void led_set_traffic(const char* led,int led_idx,const char* trigger,int on, int off)
{
	// bypass if led is held
	if(is_led_held(led,led_idx))
		return;
	
	led_put_str(led,led_idx,LED_ENTRY_TRIGGER,trigger,0);
	
	led_put_int(led,led_idx,LED_ENTRY_BRIGHTNESS_ON,on,LED_ENTRY_TIMEOUT);
	led_put_int(led,led_idx,LED_ENTRY_BRIGHTNESS_OFF,off,0);
	led_put_int(led,led_idx,LED_ENTRY_BRIGHTNESS,off,0);
}

/*
 * Set up an led to continuously beat. The beat pattern is specified by a
 * sequence of 0s and 1s with the first beat value corresponding to the least
 * significant bit. The tempo specifies how long each bit in the pattern is
 * rendered for. The pattern is considered to end at the last 1 bit (ie,
 * any trailing 0s are ignored). At the end of the pattern there is a rest
 * period after which the beat will start again from the beginning of the
 * pattern.
 *
 * Example: pattern="101111", tempo=100, rest=2000. This will turn the led
 * on for 4 beats, off for 1 beat and then on again for 1 beat where each
 * beat is 100 msecs. Then the led will be off for 2 secs and start again
 * after that.
 *
 * Parameters:
 *    led    led name
 *    led_idx    led index
 *    brightness_on    Brightness value for the led on state (0-255).
 *    brightness_off     Brightness value for the led off state (0-255).
 *    pattern    Beat bit pattern as a string sequence of '0's and '1's.
 *               Can be at most 32 characters long. Beat starts from the least
 *               significant bit.
 *    tempo    Each bit in the beat pattern is pulsed for this number of msecs.
 *    rest    Msecs to wait after the last bit in the pattern before cycling
 *            back to the beginning of the pattern. led is kept in off state
 *            during the rest time.
 */
int led_set_beat(const char *led, int led_idx, int brightness_on,
                 int brightness_off, const char *pattern, int tempo,
                 int rest)
{
    unsigned int pattern_int = 0;
    int pattern_len;
    int ix;

    if (!pattern) {
        return -EINVAL;
    }

    pattern_len = strlen(pattern);
    if (pattern_len > (sizeof(int) * 8)) {
        return -EOVERFLOW;
    }

    /* Convert the string to an integer */
    for (ix = 0; ix < pattern_len; ix++) {
        switch (pattern[ix]) {
        case '0':
            break;
        case '1':
            pattern_int |= (1 << (pattern_len - ix - 1));
            break;
        default:
            return -EINVAL;
        }
    }

    /* Update the sys values to setup the beat trigger */
    led_put_str(led, led_idx, LED_ENTRY_TRIGGER, LED_TRIGGER_BEAT, 0);
	led_put_int(led, led_idx, LED_ENTRY_BRIGHTNESS_ON, brightness_on,
                LED_ENTRY_TIMEOUT);
	led_put_int(led, led_idx, LED_ENTRY_BRIGHTNESS_OFF, brightness_off, 0);
    led_put_int(led, led_idx, LED_ENTRY_TRIGGER_PULSE, tempo, 0);
    led_put_int(led, led_idx, LED_ENTRY_TRIGGER_DEAD, rest, 0);
    led_put_int(led, led_idx, LED_ENTRY_PATTERN, pattern_int, 0);

    return 0;
}
