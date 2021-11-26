/*!
* Copyright Notice:
* Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
* CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "cdcs_syslog.h"
#include "./pots_rdb_operations.h"
#include "./fsk/packet.h"
#include "./slic_control/slic_control.h"
#include "./slic_control/tone_pattern.h"

#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
#include "ntc_pcm_cmd.h"
#endif

int pots_bridge_running = 1;	// for compile error
static struct slic_t test_slic[ MAX_CHANNEL_NO ];

// "LinpcsmR" is hidden test menu..
// "Linp" is for one/two channel loopback menu only..
#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
const char shortopts[] = "VvhtlLwd:r:i:n:p:s:m:R:cP:?";
#else
const char shortopts[] = "Vvhtl:wd:r:s:m:R:cP:?";
#endif
volatile static int running = 1;
static int fxs_fd = -1;

#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
#ifdef TWO_CHANNEL_LOOPBACK
#define PCM_CH_NO	2
#else
#define PCM_CH_NO	1
#endif

int pcm_fd[PCM_CH_NO];
int pcm_key_fd = -1;
static unsigned int bytes_to_read = 160;
static unsigned int interval = 20;
static unsigned int pcm_enable_only = 0;
#endif

#ifndef _NTC_PCM_CMD_H
typedef enum {
	PCM_CLK_CHANGE = 0,
	PCM_LOOPBACK_MODE,
	PCM_LOOPBACK_MODE_SINGLE,
	PCM_DISPLAY_DATA,
	PCM_SET_SLAVE_MODE,
	PCM_SET_MASTER_MODE,
	PCM_CHK_GPIO_PCM_CLK,
	PCM_SET_CPU_BUFFER
} pcm_drv_cmd_enum_type;
#endif

/* Version Information */
/* 1.2.0 add telephony profile option */
#define VER_MJ		1
#define VER_MN		3
#define VER_BLD		0

//#define PCM_DUMP_USING_CAT

#define	RDB_MODEM_NAME			"wwan.0.model"
static BOOL check_modem(void)
{
	char model[1024] = {0, };
	if (rdb_get_single(RDB_MODEM_NAME, model, sizeof(model)) < 0)
		model[0] = 0;
	SYSLOG_INFO("model_name=%s", model);

	if (!strcmp(model, "") || model[0] == 0)
		return FALSE;
	/* Huawei EM820U does not output PCM clock */
	if (strcmp(model, "EM820U") == 0)
		return FALSE;
	return TRUE;
}

#define	RDB_BOARD_NAME			"system.board"
static BOOL check_board_has_external_pcm_crystal(void)
{
	char board[1024] = {0, };
	if (rdb_get_single(RDB_BOARD_NAME, board, sizeof(board)) < 0)
		board[0] = 0;
	SYSLOG_DEBUG("board_name=%s", board);

	if (!strcmp(board, "3g38wv") || !strcmp(board, "3g38wv2") || !strcmp(board, "3g36wv") ||
		!strcmp(board, "3g39wv") || !strcmp(board, "3g30ap")  || !strcmp(board, "3g30apo") ||
		!strcmp(board, "elaine") || !strcmp(board, "4g100w") || !strcmp(board, "3g22wv") || !strcmp(board, "3g46"))
		return TRUE;
	return FALSE;
}

static int change_pcm_clock_mode(pcm_drv_cmd_enum_type mode, BOOL check_ext_clk_n_modem)
{
#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
	pcm_drv_cmd_enum_type new_mode = mode;
	int fd;

	if (check_ext_clk_n_modem && check_board_has_external_pcm_crystal())
	{
		slic_change_cpld_mode(CPLD_MODE_LOOP);
		return 0;
	}

	if (mode == PCM_SET_SLAVE_MODE)
	{
		if (check_ext_clk_n_modem && !check_modem())
		{
			slic_change_cpld_mode(CPLD_MODE_LOOP);
			new_mode = PCM_SET_MASTER_MODE;
		}
		else
		{
			slic_change_cpld_mode(CPLD_MODE_VOICE);
		}
	}
	else
	{
		slic_change_cpld_mode(CPLD_MODE_LOOP);
	}

	if((fd = open("/dev/PCM0",O_RDONLY)) < 0)
	{
		fprintf(stderr, "ERROR: PCM driver can not be opened!\n");
		return -1;
	}
	ioctl(fd, new_mode, 0);
	close(fd);
#endif
	return 0;
}

void exit_with_pcm_restore(void)
{
	(void) change_pcm_clock_mode(PCM_SET_SLAVE_MODE, 1);
	rdb_close_db();
	exit(255);
}

void usage(BOOL do_exit)
{
#if defined ( USE_ALSA )
	fprintf(stderr, "\nfxs_test [options]\n\n");
#else
	fprintf(stderr, "\nfxs_test [options] [device]\n\n");
#endif
	fprintf(stderr, " Options:\n");
	fprintf(stderr, "\t-d [dtmf digits (0-9, *, #, a-d, A-D)] Send DTMF digits via [device]\n");
	fprintf(stderr, "\t-r [number of rings (1-9)] Generate ring on [device]\n");
	fprintf(stderr, "\t-t Enable dialtone on [device]\n");
#if defined ( USE_ALSA )
	fprintf(stderr, "\t-l Audio loopback test on [device]\n");
	fprintf(stderr, "\t   1 : Analog Loopback Mode 1\n");
	fprintf(stderr, "\t   2 : Digital Loopback Mode\n");
	fprintf(stderr, "\t   3 : Analog Loopback Mode 2\n");
#else
#ifdef TWO_CHANNEL_LOOPBACK
	fprintf(stderr, "\t-l Audio loopback test on [device1] [device2]\n");
	fprintf(stderr, "\t   [device1] <----> [device2]\n");
#elif (defined ONE_CHANNEL_LOOPBACK)
	fprintf(stderr, "\t-l Audio loopback test on [device1]\n");
	fprintf(stderr, "\t   [device1] <----> [device1]\n");
#else
	fprintf(stderr, "\t-l Audio loopback test on [device]\n");
	fprintf(stderr, "\t   0 : Stop Loopback test\n");
	fprintf(stderr, "\t   1 : AC path Analog Codec Loopback\n");
	fprintf(stderr, "\t   2 : AC path Digital 1-bit Loopback\n");
	fprintf(stderr, "\t   3 : Hybrid Loopback\n");
	fprintf(stderr, "\t   4 : PCM Loopback\n");
#endif
#endif
	/* support international telephony profile */
	fprintf(stderr, "\t-s telephony profile\n");
	fprintf(stderr, "\t   0 : default(AU)\n");
	fprintf(stderr, "\t   1 : Canadian\n");
	fprintf(stderr, "\t   2 : North America(Int)\n");
	/* end of support international telephony profile */
	/* PCM control */
	fprintf(stderr, "\t-P PCM control\n");
	fprintf(stderr, "\t   0 : off\n");
	fprintf(stderr, "\t   1 : enable(a_law, even)\n");
	fprintf(stderr, "\t   2 : enable(16 bits linear)\n");
#if defined ( USE_ALSA )
	fprintf(stderr, " Devices: [/dev/slic]\n");
#else
	fprintf(stderr, " Devices: [/dev/slic0], [/dev/slic1]\n");
#endif
	fprintf(stderr, "\n");
	if (do_exit)
		exit_with_pcm_restore();
}

int slic_play_dialtone(slic_tone_enum tone)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
		if (slic_play(test_slic[i].slic_fd, tone) != 0)
		{
			fprintf(stderr, "fxs_test: test failed\n");
			return -1;
		}
	(tone == slic_tone_dial) ? fprintf(stderr, "fxs_test: dialtone enabled.\n") :
	(tone == slic_tone_call_waiting) ? fprintf(stderr, "fxs_test: call waiting tone enabled.\n") : fprintf(stderr, "fxs_test: tone disabled.\n");
	return 0;
}

int slic_play_dtmf_test_tones(const char* dial)
{
	int	ret = 0;
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
		ret = slic_play_dtmf_tones(test_slic[i].slic_fd, dial, FALSE);
	return ret;
}

int slic_test_loopback(int test_mode)
{
#if defined ( USE_ALSA )
	const char* loopback_mode_name[4] = { "stop Loopback test", "Analog Loopback Mode 1", "Digital Loopback Mode", "Analog Loopback Mode 2" };
#else
	const char* loopback_mode_name[5] = { "stop Loopback test", "AC path Analog Codec Loopback", "AC path Digital 1-bit Loopback", "Hybrid Loopback", "PCM Loopback" };
#endif

	int i;
	for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
		if (slic_set_loopback_mode(test_slic[i].slic_fd, test_mode) != 0)
		{
			fprintf(stderr, "fxs_test: test failed\n");
			return -1;
		}

	fprintf(stderr, "fxs_test: set LOOPBACK mode to [%s]\n", loopback_mode_name[test_mode]);
	return 0;
}

#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
int slic_test_multi_channel_loopback(int on_off)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
	{
		if (slic_set_multi_channel_loopback_mode(test_slic[i].slic_fd, on_off) != 0)
		{
			fprintf(stderr, "fxs_test: test failed\n");
			return -1;
		}
	}
	return 0;
}

static int check_loopback_condition(void)
{
	slic_on_off_hook_enum loop_state;
	int i, result;
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		result = slic_get_loop_state(test_slic[i].slic_fd, &loop_state);
		if (result == 0)
		{
			if (loop_state == slic_on_hook)
			{
				fprintf(stderr, "\nfxs_test: Error : Channel [%d] is On-Hook state\n", test_slic[i].cid);
				return -1;
			}
		}
		else
		{
			fprintf(stderr, "\nfxs_test: failed to get loop state (%s)", strerror(errno));
		}

	}
	return 0;
}
#endif

static void sig_handler(int signum)
{
	SYSLOG_INFO("caught signal %d", signum);
	switch (signum)
	{
		default:
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			running = 0;
			break;
	}
}

static int wait_for_quit(void)
{
	fprintf(stderr, "\nfxs_test: Press any key to stop test...\n\n");

	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);

	while (running)
	{
		fd_set fdr;
		int selected;
		int nfds = 1 + fxs_fd;
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
		FD_ZERO(&fdr);
		FD_SET(nfds, &fdr);
		selected = select(1 + nfds, &fdr, NULL, NULL, &timeout);
		if (!running)
		{
			break;
		}
		if (selected < 0)
		{
			SYSLOG_ERR("select() failed with error %d (%s)", selected, strerror(errno));
			return -1;
		}
		if (selected > 0)
		{
			if (FD_ISSET(nfds, &fdr))
			{
				SYSLOG_DEBUG("got event on FXS port");
				break;
			}
		}
	}
	close(fxs_fd);
	fxs_fd = -1;
	return 0;
}


#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)

#ifndef PCM_DUMP_USING_CAT

unsigned char pcm_buf[PCM_BUFFER];

static int pcm_read(int ch, unsigned char* rp, unsigned int max_count)
{
	int rd_cnt;
	rd_cnt = read(pcm_fd[ch], rp, max_count);
	if (rd_cnt <= 0) return -1;
	return rd_cnt;
}

static int pcm_write(int ch, unsigned char* wp, unsigned int cnt)
{
	int wr_cnt;
	wr_cnt = write(pcm_fd[ch], wp, cnt);
	if (wr_cnt <= 0) return -1;
	return wr_cnt;
}
#endif

//#define	READ_BUFFER_CHECK
//#define SEND_1KH_TEST_TONE
static int pcm_loopback_and_wait_for_quit(void)
{
#ifdef PCM_DUMP_USING_CAT
	fprintf(stderr, "\nfxs_test : Press Ctrl-c to stop test...\n");
#if defined(TWO_CHANNEL_LOOPBACK)
	system("cat /dev/PCM0 > /dev/PCM1 | cat /dev/PCM1 > /dev/PCM0");
#else
	system("cat /dev/PCM0 > /dev/PCM0");
#endif
#else	/* PCM_DUMP_USING_CAT */
{
	struct sigaction sa;
	int totalreads = 0;

#ifdef	READ_BUFFER_CHECK
#define BYTES_PER_FRAME		160
#define MONITOR_FRAME_CNT	10
#define MONITOR_BYTES_CNT	BYTES_PER_FRAME * (MONITOR_FRAME_CNT + 1)
//#define SKIP_FRAME_CNT		100		// 10ms * 100 = 1s
#define SKIP_FRAME_CNT		0
	unsigned long ten_ms_frame_cnt = 0;
	unsigned char frame_buf[MONITOR_BYTES_CNT];
	unsigned long total_rdcnt = 0;
	int i,j;
	memset(frame_buf, 0x00, MONITOR_BYTES_CNT);
#endif	/* READ_BUFFER_CHECK */

#ifdef SEND_1KH_TEST_TONE
	int fp;
	/* testtone.txt is a binary file contains raw pcm data of 1KHz tone.
	* This file should be located manually in /usr/bin/
	*/
	const char test_file[]="/usr/bin/testtone.txt";
	fp = open(test_file, O_RDONLY);
	if(fp < 0) {
		printf("fail to open test tone file %s\n", test_file);
		return -1;
	}
#endif

	fprintf(stderr, "\nfxs_test : Press Ctrl-c to stop test...\n");

	// These will not interrupt the 
	// read system call
	signal(SIGHUP, sig_handler);
	
	// The following signal will 
	// interrupt the read system call. 
	// Hence, it exits the main successfully
	// when the remote command socket is in use.
	memset(&sa,0,sizeof(sa));
	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa,NULL);
	sigaction(SIGTERM, &sa,NULL);

	while (running)
	{
		int rd_cnt, wr_cnt, idx;

#ifdef SEND_1KH_TEST_TONE
retry:
		rd_cnt = read(fp, (unsigned char *) & pcm_buf[0], 160);
		if (rd_cnt > 0 && pcm_buf[0] == 0x00 && pcm_buf[1] == 0x00)
			goto retry;
		if (rd_cnt < bytes_to_read){
			close(fp);
			fp = open(test_file, O_RDONLY);
			if(fp < 0) {
				printf("fail to open test tone file %s\n", test_file);
				return -1;
			}
			if (rd_cnt == 0) continue;
		}
#else
		rd_cnt = pcm_read(test_slic[0].cid, (unsigned char *) & pcm_buf[0], bytes_to_read);
#endif

#if defined(TWO_CHANNEL_LOOPBACK)
		//fprintf(stderr, "ch[0] --> ch[1] : read %d bytes\n", rd_cnt);
#else
		//fprintf(stderr, "ch[0] --> ch[0] : read %d bytes\n", rd_cnt);
#endif
		if (rd_cnt > 0)
		{
				totalreads += rd_cnt;

#ifdef	READ_BUFFER_CHECK
				if (ten_ms_frame_cnt != 0xffffffff)	ten_ms_frame_cnt++;
				if (ten_ms_frame_cnt > SKIP_FRAME_CNT && ten_ms_frame_cnt != 0xffffffff) {
					memcpy((unsigned char *)&frame_buf[total_rdcnt], (unsigned char *)&pcm_buf[0], rd_cnt);
					total_rdcnt += rd_cnt;
					if (ten_ms_frame_cnt == SKIP_FRAME_CNT + MONITOR_FRAME_CNT) {
						fprintf(stderr,"*********************************************\n");
						for (i=0;i<MONITOR_FRAME_CNT*10;i++) {
							for (j=0;j<16;j++) {
								fprintf(stderr,"%02x", frame_buf[i*16+j]);
							}
							fprintf(stderr,"\n");
						}
						fprintf(stderr,"*********************************************\n");
						ten_ms_frame_cnt = 0xffffffff;
					}
				}
#endif	/* READ_BUFFER_CHECK */

				idx = 0;
				do
				{
#if defined(TWO_CHANNEL_LOOPBACK)
					wr_cnt = pcm_write(test_slic[1].cid, (unsigned char *) & pcm_buf[idx], rd_cnt);
#else
					wr_cnt = pcm_write(test_slic[0].cid, (unsigned char *) & pcm_buf[idx], rd_cnt);
#endif
					if(wr_cnt < 0)
					{
						perror("PCM write ");
						running = 0;
						break;
					}

					if (rd_cnt >= wr_cnt)
					{
#if defined(TWO_CHANNEL_LOOPBACK)
						//fprintf(stderr, "ch[0] --> ch[1] : write [%d] bytes!\n", wr_cnt);
#else
						//fprintf(stderr, "ch[0] --> ch[0] : write [%d] bytes!\n", wr_cnt);
#endif
						rd_cnt -= wr_cnt;
						idx += wr_cnt;
					}
					else
					{
						fprintf(stderr, "Hur? rd_cnt = %d, wr_cnt %d\n", rd_cnt, wr_cnt);
						break;
					}
				}
				while (rd_cnt > 0 && running);
		}
		else
		{
			perror("PCM read 1 ");
			running = 0;
			break;
		}	

#if defined(TWO_CHANNEL_LOOPBACK)
		if(running)
		{
			rd_cnt = pcm_read(test_slic[1].cid, (unsigned char *) & pcm_buf[0], bytes_to_read);
			//fprintf(stderr, "ch[1] --> ch[0] : read %d bytes\n", rd_cnt);
			if (rd_cnt > 0)
			{
					idx = 0;
					do
					{
						wr_cnt = pcm_write(test_slic[0].cid, (unsigned char *) & pcm_buf[idx], rd_cnt);
						if(wr_cnt < 0)
						{
							perror("PCM write ");
							running = 0;
							break;
						}

						if (rd_cnt > wr_cnt)
						{
							//fprintf(stderr, "ch[1] --> ch[0] : write [%d] bytes!\n", wr_cnt);
							rd_cnt -= wr_cnt;
							idx += wr_cnt;
						}
						else
						{
							break;
						}
					}
					while (rd_cnt > 0 && running);
			}
			else
			{
				perror("PCM read 2 ");
				running = 0;
				break;
			}	
		}
#endif

	}


	printf("Total read bytes: %d\n",totalreads);
}

#endif	/* PCM_DUMP_USING_CAT */
	return 0;
}

static int pcm_loopback_and_wait_for_quit2(void)
{
	int ret;

	fprintf(stderr, "\nfxs_test : Press any key to stop test...\n");

	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	ret = ioctl(pcm_fd[0], PCM_LOOPBACK_MODE, 1);
	if (ret)
	{
		fprintf(stderr, "\nfxs_test : ioctl error %d.\n", ret);
		return ret;
	}

	while (running)
	{
		fd_set fdr;
		int selected;
		int nfds = 1 + pcm_key_fd;
		struct timeval timeout = { .tv_sec = 0, .tv_usec = interval * 1000 };

		FD_ZERO(&fdr);
		FD_SET(nfds, &fdr);
		selected = select(1 + nfds, &fdr, NULL, NULL, &timeout);
		if (!running)
		{
			break;
		}
		if (selected < 0)
		{
			SYSLOG_ERR("select() failed with error %d (%s)", selected, strerror(errno));
			return -1;
		}
		if (selected > 0)
		{
			if (FD_ISSET(nfds, &fdr))
			{
				SYSLOG_DEBUG("got event!");
				break;
			}
		}
	}
	ret = ioctl(pcm_fd[0], PCM_LOOPBACK_MODE, 0);
	close(pcm_key_fd);
	pcm_key_fd = -1;
	return 0;
}

int pcm_device_open(const char* device)
{
	int fd;
	fd = open(device, O_RDWR, 1);
	if (fd < 0)
	{
		SYSLOG_ERR("failed to open '%s' (%s)!", device, strerror(errno));
		return fd;
	}
	return fd;
}

void pcm_device_close(void)
{
	int i;
	for (i = 0; i < PCM_CH_NO; i++)
	{
		if (pcm_fd[i] < 0)
		{
			return;
		}
		close(pcm_fd[i]);
	}
}

static int open_pcm_devices(void)
{
	int i;
	char* device[2] = {"/dev/PCM0", "/dev/PCM1"};
	for (i = 0; i < PCM_CH_NO; i++)
	{
		if ((pcm_fd[i] = pcm_device_open((const char *)device[i])) < 0)
		{
			fprintf(stderr, "failed to open PCM device[%d]!\n", i);
			exit_with_pcm_restore();
		}
	}

	return 0;
}
#endif	/* defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK) */

int slic_tx_mute_override_set(int mute)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
		if (slic_tx_mute_override(test_slic[i].slic_fd, mute) != 0)
		{
			fprintf(stderr, "fxs_test: test failed\n");
			return -1;
		}
	return 0;
}

int hex2num(char c)
{
	if (c >= '0' && c <= '9')
	return c - '0';
	if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
	return -1;
}

unsigned long hexstr2int(const char *hex)
{
	int i, len = strlen(hex);
	int a;
	const char *ipos = hex;
	unsigned long ret_val = 0;
	for (i = 0; i < len; i++) {
		a = hex2num(*ipos);
		if (a < 0)
			return 0;
		ret_val = ((ret_val << 4) | a);
		ipos++;
	}
	return ret_val;
}

int slic_reg_rw_test(char *cmd_str)
{
	reg_rw_cmd_type cmd;
	int idx = 0, len = strlen(cmd_str);
	int i;
	char addr_str[5];
	char val_str[9];

	if (len > 14) return -1;
	if (cmd_str[0] == 'm') cmd.type = 0;
	else if (cmd_str[0] == 'r') cmd.type = 1;
	else return -1;

	while (idx < len && cmd_str[idx] != ',') idx++;
	if (idx > 5) return -1;

	(void) memset((char *)&addr_str[0], 0x00, 5);
	(void) memset((char *)&val_str[0], 0x00, 9);
	(void) strncpy((char *)&addr_str[0], (char *)&cmd_str[1], idx-1);
	cmd.addr = atoi(addr_str);
	fprintf(stderr, "idx %d, len %d\n", idx, len);
	if (idx == len-1)		/* read */
	{
		cmd.rw = 0;
		cmd.value = 0;
		fprintf(stderr, "get %s : %d\n", cmd.type? "reg":"mem", cmd.addr);
	}
	else
	{
		cmd.rw = 1;
		(void) strncpy((char *)&val_str[0], (char *)&cmd_str[idx+1], len-idx);
		cmd.value = hexstr2int((char *)&val_str[0]);
		fprintf(stderr, "set %s : %d : 0x%lx\n", cmd.type? "reg":"mem", cmd.addr, cmd.value);
	}
	if (cmd.addr >= 4 && cmd.addr <= 10) return -1;

	for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
		if (slic_reg_rw(test_slic[i].slic_fd, (reg_rw_cmd_type *)&cmd) != 0)
		{
			fprintf(stderr, "fxs_test: test failed\n");
			return -1;
		}
	return 0;
}

#define RDB_FXS_TEST_NAME          "pots.fxs_testing"
static void update_fxs_test_state_db(int onoff)
{
	char mode[1024] = {0, };
	const char *state[2] = {"0", "1"};
	//fprintf(stderr, "fxs_test: set '%s' to '%s'\n", RDB_FXS_TEST_NAME, state[onoff]);
	if (rdb_get_single(RDB_FXS_TEST_NAME, mode, sizeof(mode)) < 0)
	{
		if (rdb_update_single(RDB_FXS_TEST_NAME, state[onoff], CREATE, ALL_PERM, 0, 0) != 0)
			fprintf(stderr, "failed to create 'pots.fxs_testing'\n");
	} else {
		rdb_set_single(RDB_FXS_TEST_NAME, state[onoff]);
	}
}

typedef enum
{
	no_test,
	dtmf_test,
	ring_test,
	dialtone_test,
	loopback_test,
	loopback_test2,
	reg_display_mode,
	mute_override_mode,
	reg_rw_mode,
	pcm_control,
	cwtone_test
}
fxs_test_mode_type;
int main(int argc, char **argv)
{
	int argi, i;
	int	ret = 0;
#ifdef MULTI_CHANNEL_SLIC
	const char *device[MAX_CHANNEL_NO];
#else
	const char* device[MAX_CHANNEL_NO] = {"/dev/slic0"};
#endif
	int j;
	int number_of_rings = -1;
#if !defined (ONE_CHANNEL_LOOPBACK) && !defined (TWO_CHANNEL_LOOPBACK)
	int loopback_mode = -1;
#endif
	const char* dial_string = 0;
	fxs_test_mode_type test_mode = no_test;
	/* support international telephony profile */
	tel_profile_type tel_profile = MAX_PROFILE;
	/* end of support international telephony profile */
	int mute_override = -1;
	char *cmd_str = 0;
	slic_pcm_type pcm_type = slic_pcm_type_off;

	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'V':
			case 'v':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0], VER_MJ, VER_MN, VER_BLD);
				exit_with_pcm_restore();
				break;
			case 'd':
				dial_string = optarg;
				test_mode = dtmf_test;
				break;
			case 'r':
				number_of_rings = atoi(optarg);
				test_mode = ring_test;
				break;
			case 't':
				test_mode = dialtone_test;
				break;
#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
			case 'l':
				test_mode = loopback_test;
				break;
			case 'i':
				interval = atoi(optarg);
				break;
			case 'n':
				bytes_to_read = atoi(optarg);
				break;
			case 'p':
				pcm_enable_only = atoi(optarg);
				break;
			case 'L':
				test_mode = loopback_test2;
				break;
#else
			case 'l':
				loopback_mode = atoi(optarg);
				test_mode = loopback_test;
				break;
#endif
			case 'c':
				test_mode = reg_display_mode;
				break;
				/* support international telephony profile */
			case 's':
				tel_profile = (tel_profile_type)atoi(optarg);
				break;
				/* end of support international telephony profile */
			case 'm':
				mute_override = atoi(optarg);
				test_mode = mute_override_mode;
				break;
			case 'R':	/* -R r/m[addr],[value] : r12,9fff */
				cmd_str = optarg;
				test_mode = reg_rw_mode;
				break;
			case 'P':
				pcm_type = atoi(optarg);
				test_mode = pcm_control;
				break;
			case 'w':
				test_mode = cwtone_test;
				break;
			case 'h':
			case '?':
				usage(TRUE);
			default:
				break;
		}
	}

	ret = 0;
	//fprintf(stderr, "fxs_test: starting...\n");
	if (rdb_open_db() <= 0)
	{
		fprintf(stderr, "failed to open database!\n");
		goto exit_rdb;
	}
	openlog("fxs", LOG_PID | LOG_PERROR, LOG_LOCAL5);
	setlogmask(LOG_UPTO(5 + LOG_ERR));

	argi = optind;
#if !defined ( USE_ALSA )
	if (argi == argc)
	{
		closelog();
		usage(TRUE);
	}
	for (i = 0; i < MAX_CHANNEL_NO; i++) device[i] = argi == argc ? "" : argv[argi++];
#ifdef TWO_CHANNEL_LOOPBACK
	if (test_mode == loopback_test && (strcmp(device[0], "") == 0 || strcmp(device[1], "") == 0))
	{
		fprintf(stderr, "\nfxs_test: Error : This loopback test needs two device!\n");
		closelog();
		usage(TRUE);
	}
#endif
#endif

	if (change_pcm_clock_mode(PCM_SET_MASTER_MODE, 1) < 0)
	{
		goto exit_log;
	}

	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		test_slic[i].slic_fd = test_slic[i].cid = -1;
		test_slic[i].call_type = NONE;
		memcpy((void *) &test_slic[i].dev_name[0], (const void *)"", sizeof(""));
	}

	for (i = 0; i < MAX_CHANNEL_NO && device[i] != 0x00 && strcmp(device[i], "") != 0; i++)
	{
		test_slic[i].slic_fd = slic_open(device[i]);
		if (test_slic[i].slic_fd < 0)
		{
			fprintf(stderr, "fxs_test: failed to initialize SLIC [%s]!\n", device[i]);
			goto exit_slic;
		}
		if (slic_get_channel_id(test_slic[i].slic_fd, &test_slic[i].cid) < 0)
		{
			fprintf(stderr, "fxs_test: failed to get cid\n");
			goto exit_slic;
		}
		strncpy((char *) &test_slic[i].dev_name[0], (const char *) device[i], strlen(device[i]));
		//fprintf(stderr, "idx %d :slic_fd %d, cid %d, dev name %s\n", i, test_slic[i].slic_fd, test_slic[i].cid, test_slic[i].dev_name);
		/* support international telephony profile */
		if (tel_profile != MAX_PROFILE)
		{
			if (slic_set_tel_profile(test_slic[i].slic_fd, tel_profile) < 0)
			{
				fprintf(stderr, "failed to set telephony profile\n");
			}
		}
		/* end of support international telephony profile */
	}

	update_fxs_test_state_db(1);
	
	if (test_mode == pcm_control)
	{
		ret = slic_play_dialtone(slic_tone_none);
		if (pcm_type != slic_pcm_type_off)
		{
			(void) change_pcm_clock_mode(PCM_SET_SLAVE_MODE, 0);
			for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
				ret = slic_enable_pcm(test_slic[i].slic_fd, pcm_type);
			if (!ret) ret = wait_for_quit();
			for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
				ret = slic_enable_pcm(test_slic[i].slic_fd, slic_pcm_type_off);
		}
		goto exit_test;
	}

	if (test_mode == reg_rw_mode && cmd_str != 0)
	{
		ret = slic_reg_rw_test(cmd_str);
		goto exit_test;
	}

	if (test_mode == mute_override_mode && mute_override >= 0 && mute_override <= 1)
	{
		ret = slic_tx_mute_override_set(mute_override);
		goto exit_test;
	}

	if (test_mode == reg_display_mode)
	{
		for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++)
			ret = slic_display_reg(test_slic[i].slic_fd);
		goto exit_test;
	}

	if (test_mode == dtmf_test && dial_string)
	{
		ret = slic_play_dtmf_test_tones(dial_string);
	}
	else if (test_mode == ring_test)
	{
		if (number_of_rings > 9)
		{
			usage(FALSE);
			goto exit_test;
		}
		if (number_of_rings >  0)
		{
			/* append slic_led_control */
			for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++) slic_handle_pots_led(test_slic[i].cid, led_flash_on);
			for (i = 0; i < number_of_rings; i++)
			{
				for (j = 0; j < MAX_CHANNEL_NO && test_slic[j].slic_fd >= 0; j++)
					ret = slic_play_distinctive_ring(test_slic[j].slic_fd, slic_distinctive_ring_0);
				sleep(4);
			}
			/* append slic_led_control */
			for (i = 0; i < MAX_CHANNEL_NO && test_slic[i].slic_fd >= 0; i++) slic_handle_pots_led(test_slic[i].cid, led_flash_off);
		}
	}
	else if (test_mode == dialtone_test)
	{
		ret = slic_play_dialtone(slic_tone_dial);
		if (!ret) ret = wait_for_quit();
		ret = slic_play_dialtone(slic_tone_none);
	}
	else if (test_mode == cwtone_test)
	{
		ret = slic_play_dialtone(slic_tone_none);
		ret = slic_play_dialtone(slic_tone_call_waiting);
		if (!ret) ret = wait_for_quit();
		ret = slic_play_dialtone(slic_tone_none);
	}
	else if (test_mode == loopback_test)
	{
#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
		// Check the condition.
		if (check_loopback_condition())
		{
#ifdef TWO_CHANNEL_LOOPBACK
			fprintf(stderr, "\nfxs_test: Error : Both POTS ports should be offhooked!\n");
#else
			fprintf(stderr, "\nfxs_test: Error : POTS port should be offhooked!\n");
#endif
			usage(FALSE);
			goto exit_test;
		}

#ifndef PCM_DUMP_USING_CAT

		if (!pcm_enable_only)
		{
			ret = open_pcm_devices();
			if (ret < 0) goto exit_test;
		}
#endif

		ret = slic_play_dialtone(slic_tone_none);
		if (ret < 0) goto exit_mch_lb;
		ret = slic_test_multi_channel_loopback(1);
		if (ret < 0) goto exit_mch_lb;
		ret = pcm_loopback_and_wait_for_quit();
		if (ret < 0) goto exit_mch_lb;
		ret = slic_test_multi_channel_loopback(0);
		if (ret < 0) goto exit_mch_lb;
#else
#if defined ( USE_ALSA )
		if (loopback_mode < 1 || loopback_mode > 3)
#else
			if (loopback_mode < 1 || loopback_mode > 4 || device[0] == 0x00 || strcmp(device[0], "") == 0)
#endif
			{
				usage(FALSE);
				goto exit_test;
			}
			else
			{
				ret = slic_test_loopback(loopback_mode);
				if (!ret) ret = wait_for_quit();
				ret = slic_test_loopback(0);
			}
#endif
	}
#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
	else if (test_mode == loopback_test2)
	{
		if (check_loopback_condition())
		{
#ifdef TWO_CHANNEL_LOOPBACK
			fprintf(stderr, "\nfxs_test: Error : Both POTS ports should be offhooked!\n");
#else
			fprintf(stderr, "\nfxs_test: Error : POTS port should be offhooked!\n");
#endif
			usage(FALSE);
			goto exit_test;
		}
		ret = open_pcm_devices();
		if (ret < 0) goto exit_mch_lb;
		ret = slic_play_dialtone(slic_tone_none);
		if (ret < 0) goto exit_mch_lb;
		ret = slic_test_multi_channel_loopback(1);
		if (ret < 0) goto exit_mch_lb;
		ret = pcm_loopback_and_wait_for_quit2();
		if (ret < 0) goto exit_mch_lb;
		ret = slic_test_multi_channel_loopback(0);
		if (ret < 0) goto exit_mch_lb;
	}

exit_mch_lb:
#endif

exit_test:

	update_fxs_test_state_db(0);
	
#if defined(ONE_CHANNEL_LOOPBACK) || defined(TWO_CHANNEL_LOOPBACK)
	pcm_device_close();
#endif

exit_slic:
	for (i = 0; i < MAX_CHANNEL_NO; i++)  if (test_slic[i].slic_fd >= 0) slic_close(test_slic[i].slic_fd);
exit_log:
	closelog();
exit_rdb:
	fprintf(stderr, "fxs_test: %s!\n", (ret ? "failed" : "done"));
	exit_with_pcm_restore();
	exit(0);
}

/*
* vim:ts=4:sw=4:
*/
