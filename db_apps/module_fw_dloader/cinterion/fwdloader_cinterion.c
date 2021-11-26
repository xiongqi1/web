#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <stdarg.h>
#include "rdb_ops.h"

// the one and only RDB session handle
struct rdb_session *g_rdb_session=NULL;

typedef unsigned char BYTE;
typedef unsigned short WORD;

#define ANSWER_OK		0x01
#define ANSWER_RETRY	0x02
#define ANSWER_FATAL	0x03
#define ANSWER_BUSY		0x04
const char *answer_s[5]={"", "ANSWER_OK", "ANSWER_RETRY", "ANSWER_FATAL", "ANSWER_BUSY"};

#define MAX_RECORD_SIZE_WORD	65535
#define MAX_RECORD_SIZE_BYTE	0xff


int	atport_fd = -1, dlport_fd = -1, uart_if = 0, recovery_mode = 0;
FILE	*file_fp = NULL;

static void printTostdout( const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	fflush(stdout);
}

void printMsgBody(char* msg, int len)
{
	unsigned char* buf = (unsigned char *)msg;
	unsigned char buf2[256] = {0x0,};
	int i, j = len/16, k = len % 16;
	for (i = 0; i < j; i++)	{
		printTostdout("%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]);
	}
	j = i;
	for (i = 0; i < k; i++)
		sprintf((char *)buf2, "%s%02x, ", buf2, buf[j*16+i]);
	printTostdout("%s\n", buf2);
}

void usage(char **argv)
{
	fprintf(stderr, "\nUsage: %s -p AT_port -d Download_port -f Firmware -i If_type\n", argv[0]);
	fprintf(stderr, "\t-p Port for AT command interface\n");
	fprintf(stderr, "\t-d Port for firmware download\n");
	fprintf(stderr, "\t-f Firmware to download \n");
	fprintf(stderr, "\t-i interface to download [UART|USB] \n");

	fprintf(stderr, "\n");
	fflush(stderr);
}

static int OpenATPort(const char * port)
{
	struct	termios oldtio, newtio;

	if ( !port || strlen(port) == 0)
		return 1;

	atport_fd = open(port, O_RDWR | O_NOCTTY | O_TRUNC);

	if (atport_fd > 0) {
		tcgetattr(atport_fd,&oldtio);
		memcpy(&newtio, &oldtio, sizeof(struct termios));
		cfmakeraw(&newtio);
		newtio.c_cc[VMIN] = 1;
		newtio.c_cc[VTIME] = 0;
		/* It returns to 57600 bps when failed f/w uploading and waits only 1 minutes */
		printTostdout("opening port with %s bps\n", (recovery_mode? "57600":"115200"));
		cfsetospeed(&newtio, (recovery_mode? B57600:B115200));
		tcsetattr(atport_fd, TCSAFLUSH, &newtio);
	}

	if (atport_fd < 0 )
		return 1; //AT Port open failure

	return 0; // AT Port open Success
}

static void WriteToATPort(BYTE * input, int length)
{
	int writtenLen = 0, total_writtenLen = 0;
	int totalBufLen = length;

	if (atport_fd < 0 || totalBufLen <= 0)
		return;
	//printTostdout("--> %s\n", input);
	do
	{
		writtenLen = write(atport_fd, input+total_writtenLen, totalBufLen-total_writtenLen);
		total_writtenLen += writtenLen;
	} while (total_writtenLen < totalBufLen);

	return;
}

static void ClearBuffer()
{
	int nfds;
	int selected;
	
	char buf[256];
	struct	timeval timeout;
	fd_set	readfds;
	
	sleep(1);
	
	nfds = 1 + atport_fd;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	
	int read_len;

	while(1) {
		FD_ZERO(&readfds);
		FD_SET(atport_fd, &readfds);
	
		/* select */
		selected = select(nfds, &readfds, NULL, NULL, &timeout);
		
		/* break if timeout */
		if (!selected)
			break;
		
		/* break if error */
		if (selected < 0) {
			// if system call
			if (errno == EINTR) {
				printTostdout("system call detected\n");
				continue;
			}
			
			printTostdout("select() punk - error#%d(str%s)\n",errno,strerror(errno));
			break;
		}
		
		/* waste any data */
		read_len=read(atport_fd, buf, sizeof(buf));
		if(read_len<=0) {
			printTostdout("failed in reading AT port - %s\n",strerror(errno));
			break;
		}
	}
}

static int ReadResultLineFromATPort(void)
{
	#define MAX_TIMEOUT_CNT	10
	#define READ_BUF_SIZE	128

	int	readLen = 0, result = 1, nfds = 0, selected = 0;
	struct	timeval timeout;
	int	timeout_cnt=0;
	fd_set	readfds;
	char	readbuf[READ_BUF_SIZE];

	if (atport_fd < 0)
		return 1; // Failure

	memset(readbuf, 0 , READ_BUF_SIZE);
	
	while(1)
	{
		nfds = 1 + atport_fd;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_SET(atport_fd, &readfds);

		selected = select(nfds, &readfds, NULL, NULL, &timeout);

		if (selected < 0)  // ERROR
		{
			// if system call
			if (errno == EINTR)
			{
				printTostdout("system call detected\n");
				continue;
			}
			
			printTostdout("select() punk - error#%d(str%s)\n",errno,strerror(errno));
			break;
		}
		else if (selected == 0)  // Timeout
		{
			timeout_cnt++;
			if (timeout_cnt > MAX_TIMEOUT_CNT) {
				if (readLen <= 0) {
					return 1;
				} else {
					//printMsgBody(readbuf, readLen);
					readbuf[readLen] = 0;
					if (strstr(readbuf, "\r\nOK")) {
						return 0;
					}
					else if (strstr(readbuf, "\r\nERROR")) {
						return 1;
					}
				}
			}
			continue;
		}
		readLen += read(atport_fd, readbuf, READ_BUF_SIZE);
		//printMsgBody(readbuf, readLen);
		if (readbuf[0] == ANSWER_OK)
			return 0;
		if (strstr(readbuf, "\r\nOK")) {
			return 0;
		}
		else if (strstr(readbuf, "\r\nERROR")) {
			return 1;
		}
	}
	return result; // Success
}

static void ClosePort(int fd)
{
	if (fd > 0)
		close(fd);

	return;
}

#define RDB_NAME_LENGTH     (128)
#define RDB_VAL_LENGTH      (1024)
static int OpenDLPort(const char * port)
{
	struct	termios oldtio, newtio;
    char rdb_var_name[RDB_NAME_LENGTH];
    char dl_port_name[RDB_VAL_LENGTH];
	int len;

	if ( !port || strlen(port) == 0) {
		return 1;
	}

	// If the dl port name is "/dev/ttyACMx" that means
	// download port is not shown yet in ModComms so
	// wait until the download port appears rather than try to
	// open the port.
	if (strncmp(port, "/dev/ttyACMx", 12) == 0) {
		sprintf(rdb_var_name, "wwan.0.module_info.dlport");
		memset(dl_port_name, 0, sizeof(dl_port_name));
		len = sizeof(dl_port_name) - 1;
		if (rdb_get(g_rdb_session, rdb_var_name, dl_port_name, &len) != 0)
		{
			printTostdout("** fail to read %s\n", rdb_var_name);
			return 1;
		} else if (strncmp(dl_port_name, "/dev/ttyACM", 11)) {
			printTostdout("** invalid DL port %s\n", dl_port_name);
			return 1;
		}
	} else {
		strcpy(dl_port_name, port);
	}

	dlport_fd = open(dl_port_name, O_RDWR | O_NOCTTY | O_TRUNC);

	if (dlport_fd > 0) {
		tcgetattr(dlport_fd,&oldtio);
		memcpy(&newtio, &oldtio, sizeof(struct termios));
		cfmakeraw(&newtio);
		newtio.c_cc[VMIN] = 1;
		newtio.c_cc[VTIME] = 0;
		/* It returns to 57600 bps when failed f/w uploading and waits only 1 minutes */
		cfsetospeed(&newtio, (recovery_mode? B57600:B115200));
		tcsetattr(dlport_fd, TCSAFLUSH, &newtio);
	}

	if (dlport_fd < 0 )
		return 1; //Download Port open failure

	return 0; // Download Port open Success
}

static void WriteToDLPort(BYTE * input, int length)
{
	int writtenLen = 0, total_writtenLen = 0;
	int totalBufLen = length;

	if (dlport_fd < 0 || totalBufLen <= 0)
		return;

	do
	{
		writtenLen = write(dlport_fd, input+total_writtenLen, totalBufLen-total_writtenLen);
		total_writtenLen += writtenLen;
	} while (total_writtenLen < totalBufLen);

	return;
}

static void ReadFromDLPort (BYTE * answer, int wait_timeout)
{
	int	readLen = 0, nfds = 0, selected = 0;
	struct	timeval timeout;
	fd_set	readfds;

	char	readbuf[READ_BUF_SIZE];

	if (dlport_fd < 0) {
		*answer = ANSWER_FATAL;
		return; // Failure
	}

	nfds = 1 + dlport_fd;
	timeout.tv_sec = wait_timeout;
	timeout.tv_usec = 0;

	FD_ZERO(&readfds);
	FD_SET(dlport_fd, &readfds);

	selected = select(nfds, &readfds, NULL, NULL, &timeout);

	if (selected < 0)  // ERROR
	{
		*answer = ANSWER_RETRY;
		return;
	}
	else if (selected == 0)  // Timeout
	{
		*answer = ANSWER_RETRY;
		return;
	}

	memset(readbuf, 0 , READ_BUF_SIZE);
	readLen = read(dlport_fd, readbuf, READ_BUF_SIZE);
	readbuf[readLen] = 0;

	*answer = readbuf[0];
	return; // Success
}

static int OpenFWFile(const char * pathname, off_t * fileLen)
{
	struct stat sb;
	if ( !pathname || strlen(pathname) == 0)
		return 1;

	file_fp = fopen(pathname, "r+");
	
	if (file_fp == NULL)
	{
		return 1;
	}

	if (stat(pathname, &sb) == -1) {
		return 1;
	}

	* fileLen = sb.st_size;
	return 0; // success
}

static void FileReadRecord(WORD * len, BYTE * buf)
{
	BYTE recordLen[2] = {0, 0};
	WORD readLen = *len;
	WORD totalReadLen = 0, recordBodyLen = 0;
	
	if (!readLen || !buf || file_fp < 0)
	{
		*len = 0;
		return;
	}

	readLen = fread(recordLen, 1, (uart_if? 1:2), file_fp);
	if (uart_if) {
		*len = (WORD) recordLen[0];
	} else {
		memcpy(len, recordLen, 2);
	}
	//printTostdout("read record len %d\n", *len);

	if ((uart_if && *len < 1) || (!uart_if && *len < 2)) 
	{
		*len = 0;
		return;
	}

	recordBodyLen = *len;

	for(totalReadLen=0; totalReadLen < recordBodyLen; )
	{
		readLen = fread(buf+totalReadLen, 1, (recordBodyLen - totalReadLen), file_fp);
		if (readLen == EOF)
			return;
		totalReadLen += readLen;
	}

	return;
}

static int isFileEnd()
{
	if(file_fp == NULL)
		return 0;

	if(feof(file_fp))
		return 0;
	else
		return 1;
}

static void CloseFWFile(FILE * fp)
{
	if (fp != NULL)
		fclose(fp);

	return;
}

//unit milliseconds
static void Wait( int ms)
{
	usleep(ms*1000); //convert to microseconds
	return;
}

int main (int argc, char *argv[])
{
	int	ret = 0;
	const char	*at_port = NULL, *dl_port = NULL, *firmware = NULL, *if_type = NULL;
	int i;
	int stat;

	BYTE	bAnswer = 0x00;
	BYTE	bRecordData[MAX_RECORD_SIZE_WORD];
	WORD	wRecordLen;
	BYTE	bRecordLen;
	off_t	fileSize = 0, transferredLen=0;
	unsigned int prevPercent = 0, currPercent = 0;
	int	iTry ;		/* Number of tries of open virtual COM port*/

	// Parse Options
	while ((ret = getopt(argc, argv, "p:d:f:i:h")) != EOF)
	{
		switch (ret)
		{
			case 'p':
				at_port = optarg;
				break;

			case 'd':
				dl_port = optarg;
				break;

			case 'f':
				firmware = optarg;
				break;

			case 'i':
				if_type = optarg;
				break;

			case 'h':
				usage(argv);
				return 2;
				
			default:
				usage(argv);
				return 2;
		}
	}

	printTostdout("Start Firmware Download.\n\n");

	if (!at_port || !dl_port || !firmware) {
		usage(argv);
		return 2;
	}

	printTostdout("if_type = %s\n", if_type);
	if (strstr(if_type, "UART")) {
		uart_if = 1;
		printTostdout("Downloading through UART port(%s)\n\n", at_port);
	}
	
	printTostdout("Opening Firmware File.\n\n");

	if(OpenFWFile(firmware, & fileSize)) {
		printTostdout("ERROR: Firmware file open failure\n");
		return 1;
	}

	if(OpenATPort(at_port)) {
		printTostdout("ERROR: AT port open failure\n");
		return 1;
	}

    // Open RDB session
    if ((rdb_open(NULL, &g_rdb_session) < 0) || !g_rdb_session)
    {
        printTostdout("Failed to open RDB.\n\n");
        return 2;
    }

	printTostdout("Entering Firmware Download Mode.\n");

	/* clear buffer */
	stat=0;
	/* check whether the module is in recovery mode */
	ClearBuffer();
	WriteToATPort((BYTE *) "AT\r", 8);
	stat=ReadResultLineFromATPort ();
	if(stat) {
		recovery_mode = 1;
		printTostdout("The module may be in recovery mode, retry with 57600 bps\n");
		/* re-open the port with 57600 bps */
		if(OpenATPort(at_port)) {
			printTostdout("ERROR: AT port open failure\n");
			return 1;
		}
		stat = 0;
	}
	for(i=0;i<3;i++) {
		ClearBuffer();
		/* Send AT command */
		WriteToATPort((BYTE *) "AT^SFDL\r", 8);
		/* Wait for answer */
		stat=ReadResultLineFromATPort ();
		if(!stat)
			break;
	}		

	if(stat) {
		printTostdout("ERROR returned from 'AT^SFDL'\n");
		return 1;
	}

	/* Close port because the virtual COM port of the module could be disabled for a short time */
	ClosePort(atport_fd) ;

	/* Allow USB host the reenabling of the virtual COM port, wait for
	   2 seconds for powering down and detecting USB device shutdown for the
	   USB host and for module start up again */
	Wait (2000) ;

	/* Try to reopen the virtual COM port again */
	iTry = 0 ;
	do
	{
		if (!OpenDLPort (dl_port))
		{
			/* Virtual COM port is opened again */
			iTry = -1 ;
		}
		else
		{
			/* Virtual COM port is not opened, wait 100 ms */
			Wait (100) ;
			iTry++ ;
		}
	} while (0 <= iTry && 80 > iTry) ;

	if (-1 != iTry)
	{
		/* Error, virtual COM port is not opened again */
		printTostdout("TIMEOUT: Download port open Failure\n");
		rdb_close(&g_rdb_session);
		return 1;
	}

	printTostdout("Start Firmware File Transfer.\n\n");

	do
	{
		wRecordLen = (uart_if? MAX_RECORD_SIZE_BYTE:MAX_RECORD_SIZE_WORD);
		/* Read record from file */
		FileReadRecord (&wRecordLen, bRecordData) ;

		do
		{
			/* Send length of record */
			if (uart_if) {
				bRecordLen = (BYTE) wRecordLen;
				WriteToDLPort ((BYTE *) &bRecordLen, 1) ;
			} else {
				WriteToDLPort ((BYTE *) &wRecordLen, sizeof (WORD)) ;
			}
			/* Send data of record */
			WriteToDLPort (bRecordData, wRecordLen) ;
			do
			{
				/* Wait for answer */
				ReadFromDLPort (&bAnswer, 1) ;
			}
			while (ANSWER_BUSY == bAnswer) ;
			//printTostdout("received answer '%s'\n", answer_s[bAnswer]);
		}while (ANSWER_RETRY == bAnswer) ;

		if (ANSWER_OK == bAnswer) {
			transferredLen += wRecordLen+(uart_if? 1:2);

			currPercent = (unsigned int) (transferredLen * 100 / fileSize);
			
			if (prevPercent != currPercent)
			{
				prevPercent = currPercent;
				printTostdout("Progress: %3d%%, %8lld / %8lld\n", currPercent, (long long)transferredLen, (long long)fileSize);
			}

// 			printTostdout("Firmware file transferred: %lld / %lld \n", (long long)transferredLen, (long long)fileSize);
		}

	} while (isFileEnd () && ANSWER_OK == bAnswer) ;

	printTostdout("\n\nComplete....\n");

	CloseFWFile(file_fp);
	ClosePort(dlport_fd);
	rdb_close(&g_rdb_session);
	return 0;
}
