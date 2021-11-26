/*!
 * Copyright Notice:
 * Copyright (C) 2011 NetComm Pty. Ltd.
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <nvram.h>

int read_line(int fd, char *cptr, int maxlen) {
	int n, rc;
	char c, *buffer;
	buffer = cptr;
	for ( n = 1; n < maxlen; n++ ) {
		if ( (rc = read(fd, &c, 1)) == 1 ) {
			if ( c == '\n' )
				break;
			*buffer++ = c;
		} else {
			return 0;
		}
	}
	*buffer = 0;
	return n;
}

#define MAX_LINE_LENGTH	1024
int main (int argc, char **argv)
{
	char nv_file[256] = "\0";
	char line_buf[MAX_LINE_LENGTH];
	char *p_lb = (char *)&line_buf[0];
	int fp, result;
	char* pToken;
	char* pSavePtr;
	int retry_cnt = 5;

	// For help text
	if (argc >= 2) {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
            printf("\nThis command is for internal system use only.\n");
            printf("It is used for writing NV items in batch mode to save"
                   " time.\n");
            printf("Please do not run this command manually.\n\n");
            return -1;
        }
	}

	if (argc < 2) {
		printf("Usage : batch_nv filename\n");
		return -1;
	}

	(void) strcpy((char *)&nv_file[0], argv[1]);
	fp = open(nv_file, O_RDONLY);
	if(fp < 0) {
		printf("batch_nv: failed to open nv list file %s.\n", nv_file);
		return -1;
	}
	nvram_close(RT2860_NVRAM);
	nvram_init(RT2860_NVRAM);

	while (read_line(fp, p_lb, MAX_LINE_LENGTH)) {
		pToken = strtok_r(p_lb, " ", &pSavePtr);
		if (!pToken) {
			printf("batch_nv: cannot read nv item name.\n");
			goto err_ret;
		}
		printf("nv name: %s, value: %s", pToken, pSavePtr);
		result = nvram_bufset(RT2860_NVRAM, pToken, pSavePtr);
		if (result < 0) {
			printf("batch_nv: write NV item failure: %s : %s\n", pToken,
                   pSavePtr);
			goto err_ret;
		}
	}
	while (retry_cnt > 0)
	{
		result = nvram_commit(RT2860_NVRAM);
		if (result < 0) {
			printf("batch_nv: commit NV items failure, retry_cnt = %d\n",
                   retry_cnt);
			retry_cnt--;
			sleep(1);
		} else {
			break;
		}
	}
	err_ret:
	nvram_close(RT2860_NVRAM);
	close(fp);
	return 0;
}

/*
* vim:ts=4:sw=4:
*/
