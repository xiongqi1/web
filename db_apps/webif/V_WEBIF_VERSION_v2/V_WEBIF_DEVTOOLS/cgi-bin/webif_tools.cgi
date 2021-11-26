#! /usr/bin/awk -f

BEGIN {
	# HEX Table for URL Decode
	hextab["0"] = 0;	hextab["8"] = 8;
	hextab["1"] = 1;	hextab["9"] = 9;
	hextab["2"] = 2;	hextab["A"] = hextab["a"] = 10
	hextab["3"] = 3;	hextab["B"] = hextab["b"] = 11;
	hextab["4"] = 4;	hextab["C"] = hextab["c"] = 12;
	hextab["5"] = 5;	hextab["D"] = hextab["d"] = 13;
	hextab["6"] = 6;	hextab["E"] = hextab["e"] = 14;
	hextab["7"] = 7;	hextab["F"] = hextab["f"] = 15
	
	# Create ASCII table for url encoding
	for ( i=1; i<=255; ++i ) ord[ sprintf ("%c", i) "" ] = i + 0;

	print ("Content-type: text/html\n");

	split (ENVIRON["QUERY_STRING"], qry, "&");
	if ( qry[1] == "I") {	# Install
		fileName = urlDecode(qry[2]);
		destdir = urlDecode(qry[3]);

		# Don't install anything that starts with a . or has a / in it
		if (fileName !~ /^\.|\//) {
			cmd = "rdb_get system.product.title";
			if(( cmd | getline) > 0)
				product_title=$0;
			else
				exit 1;
			close (cmd);
			num = split (fileName, extstr, ".");
			if(num > 0) {
				extstr[num]=tolower(extstr[num]);
			}
			if(  num > 0 && (extstr[num]=="html" || extstr[num]=="js" || extstr[num]=="htm" || extstr[num]=="css" || extstr[num]=="inc")) {
				if( destdir=="" )
					destdir="/www"
				cmd = "mv -f /opt/cdcs/upload/"fileName " " destdir"/"fileName" >/dev/null 2>&1 &";
				system (cmd);
			}
			else if(  num > 0 && ( extstr[num]=="cgi" || extstr[num]=="lua" ) ) {
				if( destdir=="" )
					destdir="/www/cgi-bin";
				cmd = "mv -f /opt/cdcs/upload/"fileName " " destdir"/"fileName" >/dev/null 2>&1 &";
				#system( "logger '********install file**** cmd="cmd"'" );
				system (cmd);
				cmd = "sleep 1 && chmod a+x "destdir"/"fileName" >/dev/null 2>&1 &";
				system ( cmd );
			}
			else if(  num > 0 && extstr[num]=="xml" ) {
				if( fileName=="apnList.xml" ) {
					destdir="/www/cgi-bin";
				}
				else {
					if( destdir=="" )
						destdir="/www/lang/en";
				}
				cmd = "mv -f /opt/cdcs/upload/"fileName " " destdir"/"fileName" >/dev/null 2>&1 &";
				system (cmd);
			}
			else if(  num > 0 && extstr[num]=="conf" ) {
				if( fileName=="default.conf" ) {
					destdir="/etc/cdcs/conf";
					cmd = "mv -f /opt/cdcs/upload/"fileName " " destdir"/"fileName" >/dev/null 2>&1 &";
					system (cmd);
				}
			}
			else if(  num > 0 && (extstr[num]=="jpg" || extstr[num]=="gif" || extstr[num]=="png")) {
				if( fileName=="" ) {
					destdir="/www/images";
				}
				cmd = "mv -f /opt/cdcs/upload/"fileName " " destdir"/"fileName" >/dev/null 2>&1 &";
				system (cmd);
			}
			else if(  num > 0 && extstr[num]=="pdf" ) {
				if( destdir=="" )
					destdir="/opt/cdcs/doc";
				system ( "mkdir /opt/cdcs/doc 2>/dev/null");
				cmd = "mv -f /opt/cdcs/upload/"fileName " " destdir"/"fileName" >/dev/null 2>&1 &";
				system (cmd);
			}
			else if(fileName=="lang.tar") {
				system("cd /opt/cdcs/upload && tar -xvf lang.tar && rm -fr /www/lang && mv -f www/lang/ /www/ && rm -fr www && rm lang.tar");
			}
		}
		else {
			printf ("Unable to install %s\n", fileName);
		}
		system ("rdb_set upload.target_filename N/A");
		print ("<script language=Javascript> setTimeout(\"window.location = '/AppUpload.html' \", 1000); </script> ");
	}
	else if( qry[1]=="getWebFiles" || qry[1]=="compressWeb" || qry[1]=="compressLang" || qry[1]=="convertCsv" ) {
		system ( "mkdir /opt/cdcs/web 2>/dev/null");
		if( qry[1]=="compressWeb" ) {
			system("tar -cf /opt/cdcs/web/www.tar /www >/dev/null 2>&1");
			#system("ln -fs /opt/cdcs/web/www.tar /www/www.tar");
			#cmd = "ls -lh /opt/cdcs/web/www.tar";
		}
		else if( qry[1]=="compressLang" ) {
			system("tar -cf /opt/cdcs/web/lang.tar /www/lang >/dev/null 2>&1");
			#system("ln -fs /opt/cdcs/web/lang.tar /www/lang.tar");
			#cmd = "ls -lh /opt/cdcs/web/lang.tar";
		}
		else if( qry[1]=="convertCsv" ) {
			system("rdb_set xml_csv_msg ''");
		 	cmd = "cat /etc/variant.sh";
			FS="=";
			while(( cmd | getline) > 0) {
				if($1=="V_WEBIF_SPEC") {
					msg = $0 "</br>Finding string IDs..."
					print msg;
					system("rdb_set xml_csv_msg '" msg "'");
					break;
				}
			}
			system("/www/cgi-bin/xml_csv.cgi &");
			close (cmd);
			exit (0);
		}
		#system( "logger '*******1111********" qry[1] "'" );
		print ("<tr>",    
			"<th width='20%' class='align10'>File Name</th>",
			"<th width='20%' class='align10'>Date</th>",
			"<th width='20%' class='align10'>Size</th>",
			"<th width='30%' class='align10'>Action</th>",
		"</tr>");
		cmd = "ls -lh /opt/cdcs/web"
		while ((cmd | getline) > 0) {
			if ($1 !~ /^-/)	# Skip anything other than regular files
				continue;
			fileName = "";
			i = 11;
			while (i <= NF) {
				fileName = fileName $i;
				if (i++ != NF)fileName = fileName " ";
			}
			# IE will give a file name with full path like 'Z:\build-work\888_bin\root.jffs2', we only need the file name.
			num = split (fileName, newstr, "\x5C");
			if( num > 0) {
				system ( "mv '/opt/cdcs/web/"fileName"'  '/opt/cdcs/web/"newstr[num]"' >/dev/null 2>&1");
				fileName = newstr[num];
			}
			encFileName = urlEncode(fileName);

			printf ("<tr>");
			printf ("<td>%s</td>", fileName);
			printf ("<td> %s %s %s </td>", $7, $8, $10);
			printf ("<td> %s </td>", $5);
			print  ("<td>");
			printf ("<a href=\"%s\">Download</a>&nbsp;&nbsp;&nbsp;&nbsp;\n", encFileName);
			printf ("&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"/cgi-bin/webif_tools.cgi?D&%s\">Delete</a>", encFileName);
		}
		close (cmd);
	}
	else if( qry[1]=="D" ) {
		fileName = urlDecode(qry[2]);
		system( "rm /opt/cdcs/web/"fileName" >/dev/null 2>&1" );
		print ("<script language=Javascript> setTimeout(\"window.location = '/UIU.html'\", 1000); </script>");
	}
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
