#! /usr/bin/awk -f

function urlDecode (str) {
	decoded = ""
	i = 1
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
	
	print ("Content-type: text/html\n");

	raw = urlDecode( ENVIRON["QUERY_STRING"] );
	len=split (raw, arr, ";");
	
	if( index(arr[1], "lang=")==0 ) {
		print "Error! Language name not found"
		exit (0);
	}
	if( index(arr[2], "file=")==0 ) {
		print "Error! File name not found"
		exit (0);
	}
	
	# validating
	counter=0
	for(i=3; i<=len; i++) {
		len2=split (arr[i], arr2, "\"=\"");
		if(len2==2) {
			counter++;
		}
	}
	if(counter!=(len-3)) {
			print "ID counting error"
		exit (0);
	}

	lang=substr( arr[1], 6, 2 );
	
#system("mkdir /tmp/lang >/dev/null 2>&1");
#system("mkdir /tmp/lang/en >/dev/null 2>&1");
#system("mkdir /tmp/lang/fr >/dev/null 2>&1");

	FS="\"";
	updated_file_list="";

	tempFile="/tmp/temp.xml"
	
	cmd_file_list="cd /www/lang/"lang"/ && ls -1 *.xml";
	while( cmd_file_list | getline ) {
		fileName=$1;
		updated=0;
		system( "rm "tempFile " >/dev/null 2>&1 && touch "tempFile );
		printf "<po>\n" > tempFile;
		cmd_read_file="cat /www/lang/"lang"/"fileName;
		while( cmd_read_file | getline ) {
			if( NF < 5 ) {
				continue;
			}
			found=0;
			for(i=3; i<=len; i++) {
				len2=split (arr[i], arr2, "\"=\"");
				gsub(/&/, "\\&amp;", arr2[2]);
				gsub(/</, "\\&lt;", arr2[2]);
				gsub(/>/, "\\&gt;", arr2[2]);
				gsub(/\"/, "\\&quot;", arr2[2]);
				if(len2==2) {
					if($2==arr2[1]){
						printf "<message msgid=\"" arr2[1] "\" msgstr=\"" arr2[2] "\"/>\n" >> tempFile;
						found=1;
						updated++;
						break;
					}		
				}
			}
			if(!found) {
				printf $0 "\n" >> tempFile;
			}
		}
		printf "</po>\n" >> tempFile;
		close (cmd_read_file);
		close (tempFile);
		if(updated) {
			if(updated_file_list=="") {
				updated_file_list = "'"fileName"'"
			}
			else {
				updated_file_list = updated_file_list ", '"fileName"'";
			}
			system( "cp -f "tempFile" /www/lang/"lang"/"fileName );
		}
	}
	close (cmd_file_list);
	system( "rm "tempFile " >/dev/null 2>&1");
	print "var result='Done'; var files=["updated_file_list"];"
	exit (0);
}
