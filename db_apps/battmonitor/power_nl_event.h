/*
 * Define structure for params in netlink uevent
 * and prototypes for uevent parsing and searching
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
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
#ifndef _POWER_NL_EVENT_H
#define _POWER_NL_EVENT_H

/* max params in netlink uevent received */
#define MAX_NL_PARAMS 32

/* structure contains params in received netlink uevent */
typedef struct {
	/* number of params received */
	int param_counter;
	/* param table */
	char *param[MAX_NL_PARAMS];
} power_nl_event_t;

/*
 * parse received buffer and store params in a power_nl_event_t
 *
 * Parameters:
 * 	p_event:	power_nl_event_t to store parsed params
 * 	buffer:		input buffer
 * 	size:		size of buffer
 *
 * Returns:
 * 	1:		successful
 * 	0:		fail
 */
int parse_nl_event(power_nl_event_t *p_event, char *buffer, int size );

/*
 * clear a power_nl_event_t
 */
void clear_nl_event(power_nl_event_t *p_event);

/*
 * find a param in a power_nl_event_t
 *
 * Parameters:
 * 	p_event:	power_nl_event_t to find param
 * 	param_name:	param's name to find
 */
const char *power_event_find_param(power_nl_event_t *p_event, const char *param_name);

#endif
