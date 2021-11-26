#! /usr/bin/awk -f 

BEGIN 
{
	print ("Content-type: text/plain\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	system ("/bin/reboot_module.sh 2>&1 >/dev/null");
	print "Module reboot request issued.";
}