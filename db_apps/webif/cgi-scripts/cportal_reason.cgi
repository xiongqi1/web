#! /usr/bin/awk -f 

BEGIN {
	print ("Content-type: text/plain\n");
	cmd = "/usr/bin/rdb_get service.cportal.capture.reason";
	cmd | getline;
	close(cmd);

	print $1;
}
