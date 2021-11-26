/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless Limited
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <syslog.h>
#include "rdb_ops.h"

//#define DEBUG_MSG
#define MAX_LINE_LENGTH	1024
#define RDB_VAR_SMS_NEW_INDEX	"wwan.0.sms.new_msg_index"
#define SMS_FILE_INDEX_LIMIT 9999
static int get_new_msg_file_index(char *path0, char *path1)
{
	struct dirent **msg_dir_list[2];
	struct stat sb;
	char *pathname[2] = {path0, path1};
	char tmp_idx_str[6] = {0,};
	int file_cnt[2] = {0, 0}, i, j, k, found = 0, oldest_idx = SMS_FILE_INDEX_LIMIT;
	char msg_file[128] = {0,};
	long int msg_time, oldest_msg_time = 0;
	int inbox_path = 0;

	for (k = 0; k < 2; k++) {
		if (!pathname[k] || strlen(pathname[k]) == 0)
			continue;
		if (strstr(pathname[k], "inbox") || strstr(pathname[k], "incoming"))
			inbox_path = 1;
		file_cnt[k] = scandir(pathname[k], &msg_dir_list[k], 0, alphasort);
		if (file_cnt[k] < 0) {
			syslog(LOG_ERR, "%s may not exist or failed to open\n", pathname[k]);
			return -1;
		}
		#ifdef DEBUG_MSG
		syslog(LOG_ERR, "file_cnt[%d]: %d\n", k, file_cnt[k]);
		#endif
	}
	for (i = 0; i < SMS_FILE_INDEX_LIMIT; i++) {
		for (k = 0; k < 2; k++) {
			if (file_cnt[k] >= 2) {
				for (j = 0, found = 0; j < file_cnt[k]; j++) {
					if(strcmp(msg_dir_list[k][j]->d_name, ".") == 0 || strcmp(msg_dir_list[k][j]->d_name, "..") == 0)
						continue;
					if ((inbox_path == 1 && strstr(msg_dir_list[k][j]->d_name, "read")) ||
						(inbox_path == 0 && strstr(msg_dir_list[k][j]->d_name, "txmsg"))) {
						strncpy(tmp_idx_str, &msg_dir_list[k][j]->d_name[6], 5);
						if (atoi(tmp_idx_str) == i) {
							found = 1;

							/* read file creation time and update oldest file time and index */
							sprintf(msg_file, "%s/%s", pathname[k], msg_dir_list[k][j]->d_name);
							if (stat(msg_file, &sb) == -1) {
								syslog(LOG_ERR, "Failed to get status of file '%s'.\n", msg_file);
							} else {
								msg_time = (long int)sb.st_ctime;
								if (oldest_msg_time == 0 || msg_time < oldest_msg_time) {
									oldest_msg_time = msg_time;
									oldest_idx = i;
								}
								#ifdef DEBUG_MSG
								syslog(LOG_ERR, "file '%s', msg_time %ld, oldest time %ld, oldest idx %d\n", msg_file, msg_time, oldest_msg_time, oldest_idx);
								#endif
							}
							break;
						}
					}
				}
			}
			/* if target index exists, find next index */
			if (found)
				break;
		}
		/* if target index exists, find next index */
		if (found)
			continue;
		break;
	}
	free(msg_dir_list[0]);
	free(msg_dir_list[1]);
	/* If target index does not exist in local inbox nor spool inbox,
	 * then use it. This new index can be empty index or last index 99999.
	 * If there is no empty index, new message will overwrite oldest message. */
	if (i == SMS_FILE_INDEX_LIMIT) {
		syslog(LOG_ERR, "index limit reached, overwriting the oldest msg index(%d)\n", oldest_idx);
		return oldest_idx;
	}
	#ifdef DEBUG_MSG
	syslog(LOG_ERR, "new index = %d\n", i);
	#endif
	return i;
}


int main (int argc, char **argv)
{
	char pathname[2][MAX_LINE_LENGTH] = {{0, }, {0, }}, index[6] = {0, };
	int idx, i;

	// For help text
	if (argc >= 2) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            printf("\nThis command is for internal system use only.\n");
            printf("It is used for indexing SMS messages that have been"
                   " sent/received.\n");
            printf("Please do not run this command manually.\n\n");
            return -1;
        }
	}
	
	if (argc < 2) {
		syslog(LOG_ERR, "Usage : get_file_index pathname\n\n");
		return -1;
	}

	if (rdb_open_db() <= 0)
	{
		syslog(LOG_ERR, "failed to open database!\n");
		return -1;
	}

	for (i = 0; i < argc - 1; i++) {
		(void) strcpy(&pathname[i][0], argv[1 + i]);
	}
	syslog(LOG_ERR, "path name[0] = %s, path name[1] = %s\n", pathname[0], pathname[1]);
	idx = get_new_msg_file_index((char *)&pathname[0], (char *)&pathname[1]);
	//#ifdef DEBUG_MSG
	syslog(LOG_ERR, "new msg index = %05d\n", idx);
	//#endif
	if (idx >= 0) {
		sprintf(index, "%05d", idx);
		rdb_set_single(RDB_VAR_SMS_NEW_INDEX, index);
	}
	rdb_close_db();
	return 0;
}

/*
* vim:ts=4:sw=4:
*/
