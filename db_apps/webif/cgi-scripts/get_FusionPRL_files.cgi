#! /usr/bin/awk -f

BEGIN
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;
	
	system ( "/etc/init.d/rc.d/ubisetup start 1>/dev/null 2>&1" );
	# Create ASCII table for url encoding
	for ( i=1; i<=255; ++i ) ord[ sprintf ("%c", i) "" ] = i + 0;

	split (ENVIRON["QUERY_STRING"], qry, "&");
	
	if( qry[1]=="getfilelist" )
	{
		# 1st line of output from df is the header 2nd is the one we want
		"df -h /opt" | getline
		"df -h /opt" | getline
		
		printf ("<table width=\"650px\"><tr><th>Uploaded Files:</th></tr>");
		printf ("<tr><td><b>Free Space: %s </b></td></tr></table>", $4);

		print ("<table width=\"90%\">");
		print ("<tr>",    
			"<td width=\"55%\"><b>File Name</b></td>",
			"<td width=\"15%\"><b>Date</b></td>",
			"<td width=\"10%\"><b>Size</b></td>",
			"<td width=\"20%\"><b>Action</b></td>",
		"</tr>");
		j = 0;
		
		system ( "mkdir /opt/Fusion -p" );
		system ( "mkdir /opt/Fusion/prldata -p" );    
		while (("ls -lhe /opt/Fusion/prldata" | getline) > 0)
		{
			if ($1 !~ /^-/)	# Skip anything other than regular files
				continue;
			fileName = "";
			i = 11;
			while (i <= NF)
			{
				fileName = fileName $i;
				if (i++ != NF)fileName = fileName " ";
			}	
			# IE will give a file name with full path like 'Z:\build-work\888_bin\root.jffs2', we only need the file name.				
			num = split (fileName, newstr, "\x5C");	
			if( num > 0) 
			{
				system ( "mv  '/opt/Fusion/prldata/"fileName"'  '/opt/Fusion/prldata/"newstr[num]"' >/dev/null 2>&1");
				fileName = newstr[num];	
			}
			encFileName = urlEncode(fileName);
			printf ("<tr>");	
			printf ("<td width=\"55%\">%s&nbsp;&nbsp;&nbsp;&nbsp;<img src='/images/waiting.gif' width='18' height='18' id='wait%s' style='display:none'/></td>", fileName, j);		
			printf ("<td width=\"15%\"> %s %s %s </td>", $7, $8, $10);
			printf ("<td width=\"10%\"> %s </td>", $5);
			print  ("<td width=\"20%\">");
	#printf ("<a onclick='displayimg(\"wait%s\", \"''\")' href=\"/cgi-bin/FusionPRL_file_action.cgi?I&%s\">Install&nbsp;&nbsp;&nbsp;&nbsp;</a>",j, encFileName);
	printf ("<a href=\"javascript:installFile('wait%s', '%s');\">Install&nbsp;&nbsp;&nbsp;&nbsp;</a>\n", j, encFileName);
			printf ("<a onclick=\"displayimg('wait%s', '')\" href=\"/cgi-bin/FusionPRL_file_action.cgi?D&%s\">&nbsp;&nbsp;&nbsp;&nbsp;Delete</a>",j, encFileName);
			print ("</form>");
			print ("</tr>");
			j++;
		}
		print ("</table>");
	}
	else if( qry[1]=="delMsgFile" )
	{
		system( "rm /tmp/flashtoolMsg.txt >/dev/null 2>&1" );
		print ("<script language=Javascript>window.location='/modulePRLupdate.html';</script> ");
	}
	else
	{	
		print("var installMsg = \"\"\;");
		if(( "ls /tmp/flashtoolMsg.txt 2>&1" | getline) > 0)
		{
			if( index( $0, "No such file or directory")==0 )
			{
				while(( "tail /tmp/flashtoolMsg.txt" | getline) > 0)
				{
					num = split ( $0, newstr, "\r");
					for( i=0; i<=num; i++)
					{
						myStr = "";
						for ( j=1; j<=length (newstr[i]); ++j )
						{
							c = substr (newstr[i], j, 1);

							if( c == "\"" )
							{
								myStr = myStr "\\" c;
							}
							else
							{
								myStr = myStr c;
							}
						}
						if( (length(myStr)>2) )
						{
							printf( "installMsg+=\"\\n%s\";\n", myStr );
						}
					}
				}
				if( index(myStr, "Done")>0 || index(myStr, "reboot")>0)
					system( "rm /tmp/flashtoolMsg.txt >/dev/null 2>&1" ); #output once
			}
		}
	}
}

function urlEncode (str)
{
	encoded = ""
	for ( i=1; i<=length (str); ++i ) 
	{
	    c = substr (str, i, 1)
	    if ( c ~ /[a-zA-Z0-9.-]/ ) 
	    {
			encoded = encoded c		# safe character
	    } 
	    else if ( c == " " ) 
	    {
			encoded = encoded "+"	# special handling
	    } 
	    else 
	    {
			# unsafe character, encode it as a two-digit hex-number
			encoded = sprintf ( "%s%%%02X", encoded, ord[c]);
	    }
	}
	return encoded
}