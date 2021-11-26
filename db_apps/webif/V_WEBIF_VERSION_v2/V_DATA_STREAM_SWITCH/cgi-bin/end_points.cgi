#! /usr/bin/awk -f

function exec(mycmd) {
	value="";
	while( mycmd | getline ) {
		value = sprintf("%s%s:", value, $1);
		$1="";
		gsub(/^[ ]+/, "");
		# this seems to prevent breaking quotes. Output will be encoded so it is not necessary now.
		#gsub(/"/, "\\\"");
		value = sprintf("%s%s;", value, $0);
	}
	close( mycmd );
	return value;
}

# invoke base64 to encode given input field
# return encoded field
function base64Encode(field,   tempCmd,   tempFile,   cmd,   encodedField) {
	# prepare a temporary file
	tempCmd = "mktemp"
	tempCmd | getline tempFile
	close(tempCmd)
	if (tempFile == "") {
		# it should not happen, return empty string in this case
		return ""
	}
	# encode
	encodedField = ""
	cmd = "base64 | tr -d '\n' > " tempFile
	printf "%s", field | cmd
	close(cmd)
	getline encodedField < tempFile

	# delete temporary file
	tempCmd = "rm " tempFile
	system(tempCmd)

	return encodedField
}

BEGIN {
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	# CSRF token
	csrfTokenValid=0;
	if (ENVIRON["csrfToken"] != "" && ENVIRON["csrfTokenGet"] != "" && ENVIRON["csrfToken"] == ENVIRON["csrfTokenGet"]) {
		csrfTokenValid=1;
	}
	sub(/^\&csrfTokenGet=[a-zA-z0-9]+\&/, "", ENVIRON["QUERY_STRING"]);

	split (ENVIRON["QUERY_STRING"], qry, "&");
	if( qry[1] == "getList") {
	########################## End Points List ###############################
		print ("var endpoints=[");
		i=0;
		while( ( en=sprintf("rdb_get service.dsm.ep.conf.%s.name", i) | getline) > 0  && $0!="") {
			if(i>0) print(",");
			printf("{\n");
			name=$1;
			printf("\"name\":Base64.decode(\"%s\"),\n", base64Encode(name));

			cmd = sprintf("rdb_get service.dsm.ep.conf.%s.type", i);
			cmd | getline;
			type=$0;
			printf("\"type\":\"%s\",\n", $0);
			close(cmd);

			# escape all special characters as name will be used in command execution
			gsub(/[^a-zA-Z0-9]/, "\\\\&", name);

			cmd="rdb_get service.dsm.ep.conf."name". -L";

			if (type == "4") # to hide max_client in udp server endpoint, UDP-LISTEN endpoint does not support "max-children" option
				cmd=cmd" | grep -v service.dsm.ep.conf."name".max_children"
			else if (type == "11")
				# exclude anything that starts with opt_ as the list is too long otherwise
				cmd=cmd" | grep -v service.dsm.ep.conf."name".opt_"

			val=exec(cmd);
			printf("\"sum\":Base64.decode(\"%s\")\n", base64Encode(val));
			printf("}");
			i++;
			close(en);
		}
		print ("\n];\n");

		cmd = "rdb_get service.dsm.ep.validated";
		if( cmd | getline ) {
			printf("var validated=\"%s\";\n", $0);
		}
		else {
			printf("var validated=\"\";\n");
		}
		close(cmd);

		cmd = "rdb_get service.dsm.ep.error_msg";
		if( cmd | getline ) {
			printf("var error_msg=\"%s\";\n", $0);
		}
		else {
			printf("var error_msg=\"\";\n");
		}
		close(cmd);
	}
	else if( qry[1] == "setup") {
		# CSRF token must be valid
		if (csrfTokenValid != 1) {
			exit 254;
		}
		i=0;
		# remove service.dsm.ep.conf.x and keep the name list
		while( (name = sprintf("rdb_get service.dsm.ep.conf.%s.name", i) | getline) > 0  && $0!="") {
			name_list[i]=$0;
			system("dsm_tool -d -C -r service.dsm.ep.conf."i );
			i++;
		}

		# write new end points
		split (qry[2], m, ",");
		i=0;
		for( k in m ) {
			split (m[k], n, ":");
			# input must be valid.
			# name has been filtered by javascript function nameFilter ---> isNameUnsafe, so it accepts !()*-/0123456789;?ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz
			# type must be a number
			if (n[1] ~ /^[!()*\/0-9;?A-Z_a-z-]+$/ && n[2] ~ /^[0-9]+$/) {
				system( "rdb_set service.dsm.ep.conf."i".name \"" n[1] "\" -p" );
				system( "rdb_set service.dsm.ep.conf."i".type \"" n[2] "\" -p" );
				i++;
			}
		}

		# list is empty
		if(i==0) {
			# remove all items
			system("dsm_tool -d -C -r service.dsm.ep.conf" );
		}
		else {
			# remove deleted items
			for( x in name_list ) {
				find=0;
				for( k in m ) {
					split (m[k], n, ":");
					if(n[1]==name_list[x]) {
						find=1;
						break;
					}
				}
				if( !find ) {
					# escape all special characters
					gsub(/[^a-zA-Z0-9]/, "\\\\&", name_list[x]);
					system("dsm_tool -d -C -r service.dsm.ep.conf."name_list[x]".");
				}
			}
		}
		system("rdb_set service.dsm.trigger 1");
	}
	if( qry[1] == "serialList") {
	########################## Get dynamic serial port list ##########################
		serial_list="";
		for ( i=0; (cmd = sprintf("rdb_get sys.hw.class.serial.%s.location", i) | getline) > 0  && $0!=""; i++) {
			if($0=="platform") {
				if (cmd = sprintf("rdb_get sys.hw.class.serial.%s.enable", i) | getline <= 0 ) {
					break;
				}
				if ($0 != "1" ) {
					continue
				}
				st="platform";
			}
			else if($0=="gadget") {
				if (cmd = sprintf("rdb_get sys.hw.class.serial.%s.enable", i) | getline <= 0 ) {
					break
				}
				en=$0
				if (cmd = sprintf("rdb_get sys.hw.class.serial.%s.status ", i) | getline <= 0 ) {
					break
				}
				if($0=="inserted" && en==1) {
					st="inserted"
				}
				else {
					st="disabled"
				}

				# disable if the router does not have any gadget bus
				otg_status_cmd="otg-bus-status.sh | grep ' in device mode.$'"
				if( otg_status_cmd | getline <= 0 ) {
					st="disabled"
				}
				close(otg_status_cmd)
			}
			else {
				if (cmd = sprintf("rdb_get sys.hw.class.serial.%s.enable", i) | getline <= 0 ) {
					break;
				}
				en=$0
				if (cmd = sprintf("rdb_get sys.hw.class.serial.%s.status ", i) | getline <= 0 ) {
					break;
				}
				if($0=="inserted" && en==1) {
					st="inserted";
				}
				else {
					st="disabled";
				}
			}
			name=sprintf("sys.hw.class.serial.%s.name", i)
			if ("rdb_get " name | getline <= 0 ) {
				break;
			}

			if (cmd = sprintf("rdb_get sys.hw.class.serial.%s.id", i) | getline <= 0 ) {
				break;
			}
			id=$0;

			if(serial_list=="") {
				serial_list="{";
			}
			else {
				serial_list = serial_list ",";
			}

			if(st=="platform") {
				serial_list = serial_list "\"" name "\":\"" st "\""
			}
			else {
				serial_list = serial_list "\"" name "\":\"" st " " id "\""
			}
		}
		close(cmd);
		if(serial_list=="") {
			serial_list="{";
			if( "rdb_get wwan.0.host_if.1" | getline ) {
				serial_list = serial_list "\"" $0 "\":\"platform\""
			}
		}
		serial_list=serial_list dyn_serial"};";
		printf ("var host_if_list=%s\n", serial_list);
	}
}
