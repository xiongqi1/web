#! /usr/bin/awk -f 

BEGIN {
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	split (ENVIRON["QUERY_STRING"], qry, "&");

	if (qry[1] == "PRLUpdate")
	{
		system("rdb_set wwan.0.moduleconfig.cmd.status");
		system("rdb_set wwan.0.moduleconfig.cmd.cmdparam CIPRL");
		system("rdb_set wwan.0.moduleconfig.cmd.command CIupdate");
		system("sleep 1");

		cmd = "/usr/bin/rdb_get wwan.0.moduleconfig.cmd.status";
		cmd | getline;
		close(cmd);

		print ( "var QueryStatus=\"\";" );
		print ( "var TriggerStatus=\""$1"\";" );
	}
	else if (qry[1] == "FUMOUpdate")
	{
		system("rdb_set wwan.0.moduleconfig.cmd.status");
		system("rdb_set wwan.0.moduleconfig.cmd.cmdparam CIFUMO");
		system("rdb_set wwan.0.moduleconfig.cmd.command CIupdate");
		system("sleep 1");

		cmd = "/usr/bin/rdb_get wwan.0.moduleconfig.cmd.status";
		cmd | getline;
		close(cmd);

		print ( "var QueryStatus=\"\";" );
		print ( "var TriggerStatus=\""$1"\";" );
	}
	else if (qry[1] == "CIActivation")
	{
		system("rdb_set wwan.0.moduleconfig.cmd.status");
		system("rdb_set wwan.0.moduleconfig.cmd.cmdparam CIActivation");
		system("rdb_set wwan.0.moduleconfig.cmd.command CIupdate");
		system("sleep 1");

		cmd = "/usr/bin/rdb_get wwan.0.moduleconfig.cmd.status";
		cmd | getline;
		close(cmd);

		print ( "var QueryStatus=\"\";" );
		print ( "var TriggerStatus=\""$1"\";" );
	}
	else if (qry[1] == "query")
	{
		system("rdb_set wwan.0.moduleconfig.cmd.command CIstatus");

		cmd = "/usr/bin/rdb_get wwan.0.moduleconfig.cmd.statusparam";
		cmd | getline;
		close(cmd);

		print ( "var QueryStatus=\""$1"\";" );
		print ( "var TriggerStatus=\"\";" );
	}
}
