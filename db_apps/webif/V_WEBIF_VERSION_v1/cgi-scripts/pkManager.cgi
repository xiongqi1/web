#! /usr/bin/awk -f 
BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	print ("var st=[");
	j=1;
	if(( "ls /usr/lib/ipkg/status 2>&1" | getline) > 0 && index( $0, "No such file or directory")==0 ) {
		while( ("cat /usr/lib/ipkg/status" | getline) > 0 ) {
			if( $1 == "Package:" ) {
				if(j>1) print(",");
				printf("{\n");
				printf("\"Package\":\"%s\",\n", $2);
				pkName = $2;
			}
			else if( $1 == "Version:" ) {
				printf("\"Version\":\"%s\",\n", $2);
			}
			else if( $1 == "Status:" ) {
				printf("\"Status\":\"%s\",\n", $2);
			}
			else if( $1 == "Architecture:" ) {
				printf("\"Architecture\":\"%s\",\n", $2);
			}
			else if( $1 == "Installed-Time:" ) {
				printf("\"Installed_Time\":\"%s\",\n", $2);
				j++;
				printf("\"detail\":\"");
				cmd = sprintf(" /usr/lib/ipkg/info/%s.list ", pkName);
				if( ( ("ls" cmd " 2>&1") | getline) > 0 && index( $0, "No such file or directory")==0 ) {
					title = "";
					while( ( ("cat" cmd) | getline) > 0 ) {
						printf( "%s\\\\n", $0 );
						title = sprintf( "%s%s&#10;", title, $0 );
					}
					printf("\",\n\"title\":\"%s\"", title);
				}
				else {
					printf("\",\n\"title\":\"\"");
				}
				printf(",\n\"description\":\"");
				cmd = sprintf(" /usr/lib/ipkg/info/%s.control ", pkName);
				if( ( ("ls" cmd " 2>&1") | getline) > 0 && index( $0, "No such file or directory")==0 ) {
					while( ( ("cat" cmd) | getline) > 0 ) {
						if( $1 == "Description:" ) {
							# escape backslash with double backslash since we want to treat backslashes as
							# plain text 
							gsub(/\\/,"\\\\", $0);
							# escape double quote as it is a JSON stuctural character
							gsub(/\"/,"\\\"", $0);
							printf( "%s", $0 );
							break;
						}
					}
				}
				printf("\"\n}");
			}
		}
	}
	print ("\n];\n");
}
