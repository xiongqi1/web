
syslog(7,"failovernoti-head.inc: form name="+form['form_name']);

if( form['form_name']=="formFONoti" ) {
	// store notification configuration
	syslog(7,"failovernoti-head.inc: storing notification configuration - FOselect_id="+form['FOselect_id']);
	set_single('wwan.0.FailoverNoti.selectedidx='+form['FOselect_id'],"-p");
	set_single('wwan.0.FailoverNoti.DeviceName='+form['deviceName'],"-p");

	// store email configuration
	syslog(7,"failovernoti-head.inc: storing email configuration");
	set_single('wwan.0.FailoverNoti.Email_to='+form['Email_to'],"-p");
	set_single('wwan.0.FailoverNoti.Email_cc='+form['Email_cc'],"-p");
	set_single('wwan.0.FailoverNoti.Email_from='+form['Email_from'],"-p");
	set_single('wwan.0.FailoverNoti.Email_serveraddr='+form['Email_serveraddr'],"-p");
	set_single('wwan.0.FailoverNoti.Email_serverport='+form['Email_serverport'],"-p");
	set_single('wwan.0.FailoverNoti.Email_username='+form['Email_username'],"-p");
	set_single('wwan.0.FailoverNoti.Email_passwd='+form['Email_passwd'],"-p");
	// store sms configuration
	syslog(7,"failovernoti-head.inc: storing sms configuration");
	set_single('wwan.0.FailoverNoti.SMS_to='+form['SMS_to'],"-p");
	syslog(7,"SMS_to="+form['SMS_to']);
}
