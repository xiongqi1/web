#! /usr/bin/awk -f

BEGIN {
	print ("Content-type: text/html\n");

	# read V_WEBIF_SPEC
	cmd = "cat /etc/variant.sh";
	FS="=";
	spec=" ";
	csvFile="/opt/cdcs/web/string_ntc.csv";
	while(( cmd | getline) > 0) {
		if($1=="V_WEBIF_SPEC") {
			spec="spec_"$2;
			gsub(/'/, "", spec);
			csvFile="/opt/cdcs/web/string_"spec".csv";
			break;
		}
	}
	close (cmd);
	FS="\"";

	# get all IDs
	fileDir="/www/lang/en";
	system( "rm "csvFile " >/dev/null 2>&1 && touch "csvFile );
	cmd_file_list="cd "fileDir" && ls -1 *.xml";
	i=1;
	id_counter=1;
	while( cmd_file_list | getline ) {
		fileName=$1;
		if(fileName=="UIU.xml") {
			continue;
		}
		cmd_read_file="cat "fileDir"/"fileName;

		while( cmd_read_file | getline ) {
			if( NF < 5 ) {
				continue;
			}
			found=0;
			for(j=1; j<id_counter; j++) {
				if(csv_id[j]==$2) {
					found++;
					break;
				}
			}
			if(!found) {
			#### $2=id $4=String(en) ###
				csv_id[i]=$2;
				gsub(/&amp;/, "\\&", $4);
				gsub(/&lt;/, "<", $4);
				gsub(/&gt;/, ">", $4);
				gsub(/&quot;/, "\"", $4);
				csv_line[i] = csv_id[i] ";" spec ";" $4 ";";
				xml_file[i]=fileName;
				i++;
			}
		}
		id_counter=i-1;
		close (cmd_read_file)
	}
	close (cmd_file_list);
	
	fl=split( "en ar fr de it es pt cz nl tw cn jp", language_list, " " );
	for (i=1; i<id_counter; i++) {
		msg = "Processing string ID ( "i" of "id_counter" ) : </br>"csv_id[i]"\n"
		system("rdb_set xml_csv_msg '" msg "'");
		for(j=2; j<=fl; j++) {
			found=0;
			fileDir="/www/lang/"language_list[j];
			fileName=xml_file[i];
			cmd_read_file="cat "fileDir"/"fileName " 2>/dev/null";
			while( cmd_read_file | getline >0 ) {
				if( NF < 5 ) {
					continue;
				}
				if(csv_id[i]==$2) {
					gsub(/&amp;/, "\\&", $4);
					gsub(/&lt;/, "<", $4);
					gsub(/&gt;/, ">", $4);
					gsub(/&quot;/, "\"", $4);
					csv_line[i] = csv_line[i] $4 ";"
					found++;
					break;
				}
			}
			close (cmd_read_file);
			if(found) {
				continue;
			}
			else {
				csv_line[i] = csv_line[i] ";"
			}
		}
		printf csv_line[i] "\n" >> csvFile
	}
	system("ln -fs /opt/cdcs/web/"csvFile" /www/"csvFile);
	msg = "Done = "id_counter";\n"
	system("rdb_set xml_csv_msg '" msg "'");
}
