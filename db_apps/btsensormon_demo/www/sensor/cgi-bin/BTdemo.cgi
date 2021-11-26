#! /usr/bin/awk -f

function exec(mycmd) {
value="";
	while( mycmd | getline ) {
		value = sprintf("%s%s", value, $0);
	}
	close( mycmd );
	return value;
}

function demoData(prefix) {
#exec( "rdb_set xxx " prefix ".data.start_idx");
	start_idx=exec( "rdb_get " prefix ".data.start_idx");
    count=exec( "rdb_get " prefix ".data.count");
    idx=start_idx+count-1;
	if( idx=="" ) {
		print ("[0,0,0,0,0,0]"); #[type, data1, data2, data3, data4, data5]
	}
	else {
		type=exec( "rdb_get " prefix ".type");
		print ("[" type);

		for (i=idx; i>=0; i--) {
			print(",");
			data=exec( "rdb_get " prefix ".data." i ".value" );
			if(data=="") {
				print ("0");
			}
			else {
				print (data);
			}
		}
		print ("]");
	}

}

BEGIN
{
	print ("Content-type: text/html\n");
	maxItems=5; //fixed 5 data history

	envStr=ENVIRON["QUERY_STRING"];

	print ("[");

	for (x=0; x<3; x++) {
		if(x>0) {
			print (",");
		}
		demoData( envStr"."x );
	}

	print ("]\n");

}
