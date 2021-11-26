#! /usr/bin/awk -f

# Read V_ variable value
function get_v_var(v_var) {
	sed_cmd="cat /etc/variant.sh | sed -n \"s/^" v_var "='\\(.*\\)'$/\\1/p\""
	if ( (sed_cmd | getline value) <= 0)
		value=""
	close(sed_cmd)
	return value
}

BEGIN {
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	# CSRF token must be valid on V_WEBIF_VERSION v2
	### Get V - variable V_WEBIF_VERSION
	cmd = "cat /etc/variant.sh 2> /dev/null"
	FS = "="
	while( cmd | getline ) {
		if ( $1 == "V_WEBIF_VERSION") {
			V_WEBIF_VERSION = $2;
			break;
		}
	}
	close(cmd);

	if (V_WEBIF_VERSION == "'v2'") {
		# CSRF token must be valid
		if (ENVIRON["csrfToken"] == "" || ENVIRON["csrfTokenGet"] == "" || ENVIRON["csrfToken"] != ENVIRON["csrfTokenGet"]) {
			exit 254;
		}
	}
	sub(/^\&csrfTokenGet=[a-zA-z0-9]+\&/, "", ENVIRON["QUERY_STRING"]);

	# HEX Table for URL Decode
	hextab["0"] = 0;	hextab["8"] = 8;
	hextab["1"] = 1;	hextab["9"] = 9;
	hextab["2"] = 2;	hextab["A"] = hextab["a"] = 10
	hextab["3"] = 3;	hextab["B"] = hextab["b"] = 11;
	hextab["4"] = 4;	hextab["C"] = hextab["c"] = 12;
	hextab["5"] = 5;	hextab["D"] = hextab["d"] = 13;
	hextab["6"] = 6;	hextab["E"] = hextab["e"] = 14;
	hextab["7"] = 7;	hextab["F"] = hextab["f"] = 15

	system ("rdb_set upload.file_size 0");
	system ("rdb_set upload.current_size 0");

	print ("Content-type: text/html\n");
	split (ENVIRON["QUERY_STRING"], qry, "&");
	if ( qry[1] == "I") {	# Install
		fileName = urlDecode(qry[2]);
		destdir = urlDecode(qry[3]);
		#system( "logger \"&&&&&&&&&------fileName="fileName "\"");
		# Don't install anything that starts with a . or has a / in it
		if (fileName !~ /^\.|\//) {
			num = split (fileName, extstr, ".");
			if(num > 0) {
				extstr[num]=tolower(extstr[num]);
			}
			# clear configured flag only for .cdi file
			if (extstr[num] == "cdi") {
				# clear smstools.configured flag before uploading f/w in order to create
				# sms configure files after uploading over
				cmd = "rdb_set smstools.configured";
				system (cmd);
			}
			system("install_file /opt/cdcs/upload/'"fileName"'");
		}
		else {
			printf ("Unable to install %s\n", fileName);
		}
		system ("rdb_set upload.target_filename N/A");
		# ipk is being installed in background so don't push "Done" to message file which will stop
		# message dumping to WEBUI
		if (extstr[num] != "ipk" && extstr[num] != "key" && extstr[num] != "crt" && (extstr[num] != "cdi" || match(fileName,"_r.cdi$"))) {
			print "Done" >> "/tmp/flashtoolMsg.txt";
		}
		print ("<script language=Javascript> setTimeout(\"window.location = '/AppUpload.html' \", 1000); </script> ");
	}
	else if ( qry[1] == "U") {	# Uninstall
		fileName = urlDecode(qry[2]);
		# Don't uninstall anything that starts with a . or has a / in it
		if (fileName !~ /^\.|\//) {
			printf ("Uninstalling package: %s\n", fileName);

			# Different process for Fisher which has Read-Only rootfs.
			v_partition_layer=get_v_var("V_PARTITION_LAYOUT")
			if( v_partition_layer == "fisher_ab") {
				cmd = "flashtool --remove-ipk '"fileName"' >/dev/null";
			} else {
				cmd = "ipkg-cl remove '"fileName"' ";
			}

			system (cmd);
		}
		else {
			printf ("Unable to uninstall %s\n", fileName);
		}
		#print ("<script language=Javascript> setTimeout(\"window.location = '/AppUpload.html' \", 1000); </script>");
	}
	else if ( qry[1] == "D") { # Delete
		fileName = urlDecode(qry[2]);
		# Don't delete anything that starts with a . or has a / in it
		if (fileName !~ /^\.|\//) {
		#	printf ("Deleting %s <p>\n", fileName);
			cmd = "rm '/opt/cdcs/upload/" fileName "'";
			system (cmd);
		}
		else
			printf ("Unable to delete %s\n", fileName);
		#print ("<script language=Javascript> setTimeout(\"window.location = '/AppUpload.html'\", 1000); </script>");
	}
	else if( qry[1]=="delPDFfile" ) {
		fileName = urlDecode(qry[2]);
		cmd="rm /opt/cdcs/doc/'"fileName"' 2>/dev/null"
		system( cmd );
		print ("<script language=Javascript>window.location='/help.html';</script>");
	}
}

function urlDecode (str) {
	decoded = ""
	i   = 1
	len = length (str)
	while ( i <= len ) {
		c = substr (str, i, 1)
		if ( c == "%" ) {
			if ( i+2 <= len ) {
				c1 = substr (str, i+1, 1)
				c2 = substr (str, i+2, 1)
				if ( hextab[c1] != "" && hextab[c2] != "" ) {
					code = 0 + hextab[c1] * 16 + hextab[c2] + 0
					c = sprintf ("%c", code)
					i = i + 2
				}
			}
		}
		else if ( c == "+" ) {
			# special handling: "+" means " "
			c = " "
		}
		decoded = decoded c
		++i
	}
	return decoded
}
