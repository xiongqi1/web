#!/usr/bin/awk -f

function exec(mycmd) {
value="";
	#system( "logger \"&&&&&&&&&------cmd="mycmd "\"");
	while( mycmd | getline ) {
		value = sprintf("%s%s", value, $0);
	}
	close( mycmd );
	return value;
}

function logger(msg) {
	system("logger -t process_predefined_command.awk << __EOF__\n" msg)
};

BEGIN {
# ARGV[1]=mode, ARGV[2]=command
#system( "logger \"&&-----ARGV[1]="ARGV[1] "-----ARGV[2]=" ARGV[2]");
	if( ARGC>=1 && ARGV[1]=="help" ) {
		print "This is AWK script is for internal system use only."
		print "It is used by the SMS Diagnostic functions."
        print "Please do not run this script manually."
		exit (0);
	}

	if( ARGC == 1 ) {
		#special zero length sms used for Wake-up message
		ARGV[1]="set";
		ARGV[2]="zerosms=";
	}
	else if( ARGC < 2 ) {
		print "error" ARGC;
		exit (-1);
	}

	FS = ";"
	COMMAND_CONF_FILE="/usr/bin/sms_commands.conf"

	retvar = "none";

	cmd_num=split (ARGV[2], cmd_arg, " ");

	# the set commands must have the equal '=' sign
	if( ARGV[1]=="set" ) {
		num = split (cmd_arg[1], s, "=");
		if(num != 2) {
			break;
		}
		command=s[1];
		EX_ARG[1]=s[2];
		EX_ARG[2]=cmd_arg[3];
		EX_ARG[3]=cmd_arg[4];
		cmd_num=3;
	}
	else {
		command=cmd_arg[1];
		EX_ARG="";
		for (i=2; i<=cmd_num; i++) {
			EX_ARG[i-1]=cmd_arg[i];
			if(EX_ARG=="") {
				EX_ARG=cmd_arg[i];
			}
			else {
				EX_ARG=EX_ARG" "cmd_arg[i];
			}
			#system( "logger \"&&&-----command="command"-----cmd_arg["i"]="cmd_arg[i] "\"");
		}
	}
	gsub(/'/g, "", EX_ARG);
	gsub(/\"/g, "", EX_ARG);
	#system( "logger \"&&&-----command="command"-----EX_ARG="EX_ARG "\"");
	while( "cat "COMMAND_CONF_FILE | getline ) {
#		$1=mode  $2=command $3=action
		if( substr($0,1,1) == "#" || NF < 3 )
			continue;

		if( command==$2 && ARGV[1]==$1) {
			if( ARGV[1]=="get" || ARGV[1]=="set" || ARGV[1]=="execute") {
				len=NF;

				# get cmd
				cmd=$0
				gsub(/^[^;]*;[^;]*;/,"",cmd)

				# replace parameters TODO: use a word matching
				for (j=1; j<=cmd_num; j++) {
					gsub("EX_ARG" j, EX_ARG[j],cmd)
				}
				# replace EX_ARG with the first argument 
				gsub("EX_ARG", EX_ARG,cmd)
				
				logger("run cmd... " cmd)

				# launch and merge output
				buf=""
				while( cmd | getline ) {
					buf = sprintf("%s%s", buf, $0);
				}
				retcode=close( cmd );
				logger( "result=\"" buf "\"");

				# get reply sms
				reply_sms=buf
				gsub(/\"/, "", reply_sms);

				# build result
				retvar = sprintf("extra_error='%d' extra_mode=%s extra_command=%s extra_reply_sms=\"%s\"", retcode, ARGV[1], command, reply_sms);
				logger( "retvar=\"" retvar "\"")
				break;
			}
			else {
				break;
			}
		}
	}
	close( cmd );
	print retvar;
	exit (0);
}
