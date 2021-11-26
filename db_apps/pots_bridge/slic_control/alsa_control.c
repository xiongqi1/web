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

//#include <stdio.h>
//#include <stdlib.h>
//#include <syslog.h>
//#include <signal.h>
#include <alsa/asoundlib.h>
#include "cdcs_syslog.h"
#include "./alsa_control.h"


static snd_hctl_t* control_handle_ = NULL;
static int is_open_ = 0;
static const unsigned int polling_interval_msec = 250;

snd_hctl_t* alsa_control_handle()
{
	return control_handle_;
}

snd_hctl_elem_t* alsa_find_elem(const char* name)
{
	snd_hctl_elem_t *elem;
	snd_ctl_elem_id_t *id = alloca(snd_ctl_elem_id_sizeof());
	memset(id, 0, snd_ctl_elem_id_sizeof());
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, name);
	elem = snd_hctl_find_elem(control_handle_, id);
	if (!elem)
	{
		SYSLOG_ERR("control element %s not found", name);
	}
	return elem;
}

int alsa_register_snd_card_ctl_callback(const char *name, snd_hctl_elem_callback_t callback, void *data)
{
	snd_hctl_elem_t *elem = alsa_find_elem(name);
	if (!elem)
	{
		SYSLOG_ERR("could find element for callback for '%s'", name);
		return -1;
	}
	snd_hctl_elem_set_callback(elem, callback);
	if (data)
	{
		snd_hctl_elem_set_callback(elem, data);
	}
	return 0;
}


// Set ALSA CARD Control.
//
// param:	Control name (ASCII String)
// v1:		Value to set control to
// v2:		Optional 2nd value for controls that require 2 values
//
int alsa_set_snd_card_ctl(char *name, int v1, int v2)
{
	int	type;
	int	err = 0;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_info_t *info;

	id = alloca(snd_ctl_elem_id_sizeof());
	memset(id, 0, snd_ctl_elem_id_sizeof());
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, name);
	elem = snd_hctl_find_elem(control_handle_, id);
	if (!elem)
	{
		fprintf(stderr, "Could not find control element '%s'\n", name);
		return -1;
	}
	info = alloca(snd_ctl_elem_info_sizeof());
	memset(info, 0, snd_ctl_elem_info_sizeof());
	snd_hctl_elem_info(elem, info);
	type = snd_ctl_elem_info_get_type(info);
	control = alloca(snd_ctl_elem_value_sizeof());
	memset(control, 0, snd_ctl_elem_value_sizeof());
	snd_ctl_elem_value_set_id(control, id);

	switch (type)
	{
		case SND_CTL_ELEM_TYPE_INTEGER:
			snd_ctl_elem_value_set_integer(control, 0, v1);
			if (v2 > 0) snd_ctl_elem_value_set_integer(control, 1, v2);
			break;
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			snd_ctl_elem_value_set_boolean(control, 0, (v1 != 0));
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
			SYSLOG_ERR("Cannot write integer to control of type 'bytes'");
			return -1;
	}
	if ((err = snd_hctl_elem_write(elem, control)) == 0)
	{
		return 0;
	}
	SYSLOG_ERR("Could not write control element (%s)", snd_strerror(err));
	return -1;
}

int alsa_set_snd_card_ctl_bytes(char *name, void* data, unsigned int size)   // quick and dirty: copy-paste
{
	int	err = 0;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_info_t *info;

	id = alloca(snd_ctl_elem_id_sizeof());
	memset(id, 0, snd_ctl_elem_id_sizeof());
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, name);
	elem = snd_hctl_find_elem(control_handle_, id);
	if (!elem)
	{
		SYSLOG_ERR("Could not find control element '%s'\n", name);
		return -1;
	}
	info = alloca(snd_ctl_elem_info_sizeof());
	memset(info, 0, snd_ctl_elem_info_sizeof());
	snd_hctl_elem_info(elem, info);
	if (snd_ctl_elem_info_get_type(info) != SND_CTL_ELEM_TYPE_BYTES)
	{
		SYSLOG_ERR("Cannot write bytes to control of non-bytes type");
		return -1;
	}
	control = alloca(snd_ctl_elem_value_sizeof());
	memset(control, 0, snd_ctl_elem_value_sizeof());
	snd_ctl_elem_value_set_id(control, id);
	snd_ctl_elem_set_bytes(control, data, size);
	if ((err = snd_hctl_elem_write(elem, control)) == 0)
	{
		return 0;
	}
	SYSLOG_ERR("Could not write control element (%s)", snd_strerror(err));
	return -1;
}

//
// Get ALSA CARD Control.
//
// hctl:		Control interface handle from snd_hctl_open()
// param:	Control name (ASCII String)
// v1:		Value to set control to
//
int alsa_get_snd_card_ctl(char *name, int *v1)
{
	int	type;
	int	err = 0;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_info_t *info;

	id = alloca(snd_ctl_elem_id_sizeof());
	memset(id, 0, snd_ctl_elem_id_sizeof());
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
	snd_ctl_elem_id_set_name(id, name);
	elem = snd_hctl_find_elem(control_handle_, id);
	if (!elem)
	{
		fprintf(stderr, "Could not find control element %s\n", name);
		return -1;
	}
	info = alloca(snd_ctl_elem_info_sizeof());
	memset(info, 0, snd_ctl_elem_info_sizeof());
	snd_hctl_elem_info(elem, info);
	type = snd_ctl_elem_info_get_type(info);
	control = alloca(snd_ctl_elem_value_sizeof());
	memset(control, 0, snd_ctl_elem_value_sizeof());
	snd_ctl_elem_value_set_id(control, id);

	if ((err = snd_hctl_elem_read(elem, control)) < 0)
	{
		SYSLOG_ERR("Control %s element read error: %s", name, snd_strerror(err));
		return err;
	}
	switch (type)
	{
		case SND_CTL_ELEM_TYPE_INTEGER:
			*v1 = snd_ctl_elem_value_get_integer(control, 0);
			break;
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			*v1 = (snd_ctl_elem_value_get_integer(control, 0) != 0);
			break;
	}
	return 0;
}

void alsa_handle_events()
{
	if (snd_hctl_wait(control_handle_, polling_interval_msec) <= 0)
	{
		return;
	}
	snd_hctl_handle_events(control_handle_);
}

int alsa_get_elem_value(snd_hctl_elem_t *elem, int* value)
{
	int	err;
	snd_ctl_elem_value_t *control = alloca(snd_ctl_elem_value_sizeof());
	memset(control, 0, snd_ctl_elem_value_sizeof());
	if ((err = snd_hctl_elem_read(elem, control)) != 0)
	{
		SYSLOG_ERR("failed to get value from element: %s", snd_strerror(err));
		return err;
	}
	*value = snd_ctl_elem_value_get_integer(control, 0);
	//SYSLOG_DEBUG("got element value: %i", *value);
	return 0;
}

snd_hctl_t *alsa_open(const char *card)
{
	int ret;
	if (is_open_)
	{
		return control_handle_;
	}
	if ((ret = snd_hctl_open(&control_handle_, card, 0)) < 0)
	{
		fprintf(stderr, "cannot open audio device %s (%s)\n", card, snd_strerror(ret));
		return NULL;
	}
	if ((ret = snd_hctl_nonblock(control_handle_, 1)) < 0)
	{
		fprintf(stderr, "cannot set audio device %s to nonblocking mode (%s)\n", card, snd_strerror(ret));
		snd_hctl_close(control_handle_);
		return NULL;
	}
	if ((ret = snd_hctl_load(control_handle_)) < 0)
	{
		fprintf(stderr, "Could not load control elements (%s)\n", snd_strerror(ret));
		snd_hctl_close(control_handle_);
		return NULL;
	}
	is_open_ = 1;
	return control_handle_;
}

void alsa_close()
{
	if (is_open_ == 0)
	{
		return;
	}
	is_open_ = 0;
	snd_hctl_close(control_handle_);
}

