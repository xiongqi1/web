#! /usr/bin/awk -f

BEGIN
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	# call ubisetup when necessary
	cmd = sprintf("rdb_get system.mounts.usrlocal");
	cmd | getline;
	usrl_mounted=$0;
	cmd = sprintf("rdb_get system.mounts.opt");
	cmd | getline;
	opt_mounted=$0;
	if ( usrl_mounted!="1" || opt_mounted!="1" ) {
		system ( "/etc/init.d/rc.d/ubisetup start 1>/dev/null 2>&1" );
	}

	# Create ASCII table for url encoding
	for ( i=1; i<=255; ++i ) ord[ sprintf ("%c", i) "" ] = i + 0;

	split (ENVIRON["QUERY_STRING"], qry, "&");

	# Full date & time option per busybox version
	# -e : 1.19.3
	# --full-time : 1.30.1~
	cmd = "busybox";
	cmd | getline;
	if ($2=="v1.19.3") {
		ls_opt = " -e ";
		old_busybox = true;
	} else {
		ls_opt = " --full-time ";
		old_busybox = false;
	}
	close(cmd);

	if( qry[1]=="getfilelist" ) {
		# 1st line of output from df is the header 2nd is the one we want
		cmd = "df -h /opt";
		cmd | getline;
		cmd | getline;
		printf ("<table width=\"650px\"><tr><th>Uploaded Files:</th></tr>");
		printf ("<tr><td><b>Free Space: %s </b></td></tr></table>", $4);
		close(cmd);
		cmd = "df -k /opt";
		cmd | getline;
		cmd | getline;
		system ( "rdb_set upload.free_size "$4 );
		close(cmd);
		print ("<table width=\"90%\">");
		print ("<tr>",
			"<td width=\"55%\"><b>File Name</b></td>",
			"<td width=\"15%\"><b>Date</b></td>",
			"<td width=\"10%\"><b>Size</b></td>",
			"<td width=\"20%\"><b>Action</b></td>",
		"</tr>");
		j = 0;

		system ( "mkdir /opt/cdcs -p" );
		system ( "mkdir /opt/cdcs/upload -p" );
		cmd = "ls -lh " ls_opt "/opt/cdcs/upload"
		while ((cmd | getline) > 0) {
			if ($1 !~ /^-/)	# Skip anything other than regular files
				continue;
			fileName = "";
			if (old_busybox) {
				i = 11;
			} else {
				i = 9;
			}
			while (i <= NF) {
				fileName = fileName $i;
				if (i++ != NF)fileName = fileName " ";
			}
			# IE will give a file name with full path like 'Z:\build-work\888_bin\root.jffs2', we only need the file name.
			num = split (fileName, newstr, "\x5C");
			if( num > 0) {
				system ( "mv  '/opt/cdcs/upload/"fileName"'  '/opt/cdcs/upload/"newstr[num]"' >/dev/null 2>&1");
				fileName = newstr[num];
			}
			encFileName = urlEncode(fileName);
			printf ("<tr>");
			printf ("<td width=\"55%\">%s&nbsp;&nbsp;&nbsp;&nbsp;<img src='/images/waiting.gif' width='18' height='18' id='wait%s' style='display:none'/></td>", fileName, j);
			if (old_busybox) {
				printf ("<td width=\"15%\"> %s %s %s </td>", $7, $8, $10);
			} else {
				printf ("<td width=\"15%\"> %s %s %s </td>", $6, $7, $8);
			}
			printf ("<td width=\"10%\"> %s </td>", $5);
			print  ("<td width=\"20%\">");
			printf ("<a class='upload' href=\"javascript:installFile('wait%s', '%s');\">Install</a>&nbsp;&nbsp;&nbsp;&nbsp;\n", j, encFileName);
			printf ("&nbsp;&nbsp;&nbsp;&nbsp;<a class='upload' href=\"javascript:deleteFile('%s')\">Delete</a>", encFileName);
			print ("</form>");
			print ("</tr>");
			j++;
		}
		print ("</table>");
		close (cmd);
	}
	else if( qry[1]=="delMsgFile" ) {
		system( "rm /tmp/flashtoolMsg.txt >/dev/null 2>&1" );
		print ("<script language=Javascript>window.location='/AppUpload.html';</script> ");
	}
	else if( qry[1]=="getPDFfilelist" ) {
		print "var doclist=["
		cmd = "ls -Llh " ls_opt "/www/doc/ 2>/dev/null"
		filecounter=0;

		while ((cmd | getline) > 0) {
			if ( !index($0, ".pdf") && !index($0, ".PDF"))
				continue;
			if(filecounter)
				print ", "
			if (old_busybox) {
				fn=$11;
				for(i=12; i<=NF; i++) {
					fn=fn" "$i;
				}
				print "{\"size\":\""$5"\", \"date\":\""$7" "$8" "$10" "$9"\", \"name\":\""fn"\"}";
			} else {
				fn=$9;
				for(i=10; i<=NF; i++) {
					fn=fn" "$i;
				}
				print "{\"size\":\""$5"\", \"date\":\""$6" "$7" "$8"\", \"name\":\""fn"\"}";
			}
			filecounter++;
		}
		print "];"
		close (cmd);
	}
	else {
		print("var installMsg = \"\"\;");
		if(( "ls /tmp/flashtoolMsg.txt 2>&1" | getline) > 0) {
			if( index( $0, "No such file or directory")==0 ) {
				while(( "tail /tmp/flashtoolMsg.txt" | getline) > 0) {
					num = split ( $0, newstr, "\r");
					for( i=0; i<=num; i++) {
						myStr = "";
						for ( j=1; j<=length (newstr[i]); ++j ) {
							c = substr (newstr[i], j, 1);
							if( c == "\"" ) {
								myStr = myStr "\\" c;
							}
							else {
								myStr = myStr c;
							}
						}
						if( (length(myStr)>2) ) {
							printf( "installMsg+=\"\\n%s\";\n", myStr );
						}
					}
				}
				if( index(myStr, "Done")>0 || index(myStr, "reboot")>0)
					system( "rm /tmp/flashtoolMsg.txt >/dev/null 2>&1" ); #output once
			}
		}
	}
	system ("rdb_set upload.file_size 0");
	system ("rdb_set upload.current_size 0");
}

function urlEncode (str) {
	encoded = ""
	for ( i=1; i<=length (str); ++i ) {
		c = substr (str, i, 1)
		if ( c ~ /[a-zA-Z0-9.-]/ ) {
			encoded = encoded c		# safe character
		}
		else if ( c == " " ) {
			encoded = encoded "+"	# special handling
		}
		else {
			# unsafe character, encode it as a two-digit hex-number
			encoded = sprintf ( "%s%%%02X", encoded, ord[c]);
		}
	}
	return encoded
}
