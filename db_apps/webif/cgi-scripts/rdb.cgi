#! /usr/bin/awk -f

BEGIN {

# CSRF token must be valid on V_WEBIF_VERSION v2
### Get V_WEBIF_VERSION
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

# accept (read only) list without login
accept_list_r="service.pppoe.server.0.enable wwan.0.radio.information.signal_strength wwan.0.mhs.docked mhs.operationmode wizard_status webinterface.language webinterface.lang_jp";
accept_list_w="mhs.operationmode wizard_status webinterface.language webinterface.lang_jp";
	print ("Content-type: text/html\n");
	num = split (ENVIRON["QUERY_STRING"], qrystr, "&");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) {
		fl_r=split( accept_list_r, accept_rdb, " " );
		for( i=1; i<=num; i++ ) {
			found=0;
			for(j=1; j<=fl_r; j++) {
				split (qrystr[i], data, "=");
				if(data[1]==accept_rdb[j]) {
					found=1;
					break;
				}
			}
			if(!found) {
				exit;
			}
		}
	}
	for( i=1; i<=num; i++ ) {
		if( ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] || ENVIRON["user"]!="root") {
			if( index(qrystr[i], "Pass")>0 || index(qrystr[i], "key")>0  || index(qrystr[i], "pin")>0 || index(qrystr[i], "admin")>0 )
				continue; # security lockup
		}
		if( (split (qrystr[i], data, "=") == 1) && (data[1] ~ /^[a-zA-Z0-9_\.]+$/) ) {
			val="";
			cmd="rdb_get "data[1];
			cmd |getline val;
			gsub("\\.","_",data[1]);
			print data[1]"=\""val"\";";
			close (cmd);
		}
		else {
			if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) {
				fl_w=split( accept_list_w, accept_rdb_w, " " );
				found=0;
				for(k=1; k<=fl_w; k++) {
					if(data[1]==accept_rdb_w[k]) {
						found=1;
						break;
					}
				}
				if(!found) {
					continue;
				}
			}
			gsub("%22","\"",data[2])
			gsub("%27","\'",data[2])
			gsub("%3D","=",data[2])
			gsub("%20-p"," -p",data[2])
			gsub("%20--%20"," -- ",data[2])

			#system( "logger \"cgi-bin-----data[1]="data[1] "-----data[2]=" data[2]"\"");
			# RDB variable name must be valid to proceed
			if (data[1] ~ /^[a-zA-Z0-9_\.]+$/) {
				# escape all special characters
				## - and space character are exempted from escaping to support "-p" and "--". 
				## So it is not possible to set the value that includes space characters as original implementation.
				gsub(/[^a-zA-Z0-9\-\ ]/, "\\\\&", data[2]);
				cmd="rdb_set "data[1]" "data[2];
				cmd |getline;
				close (cmd);
			}
		}
	}
	exit;
}
