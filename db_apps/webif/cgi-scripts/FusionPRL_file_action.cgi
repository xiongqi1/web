#! /usr/bin/awk -f 
BEGIN 
{
	print ("Content-type: text/html\n");
	if( ENVIRON["SESSION_ID"]=="" || ENVIRON["SESSION_ID"] !=  ENVIRON["sessionid"] ) exit;

	# HEX Table for URL Decode
	hextab["0"] = 0;	hextab["8"] = 8;
	hextab["1"] = 1;	hextab["9"] = 9;
	hextab["2"] = 2;	hextab["A"] = hextab["a"] = 10
	hextab["3"] = 3;	hextab["B"] = hextab["b"] = 11;
	hextab["4"] = 4;	hextab["C"] = hextab["c"] = 12;
	hextab["5"] = 5;	hextab["D"] = hextab["d"] = 13;
	hextab["6"] = 6;	hextab["E"] = hextab["e"] = 14;
	hextab["7"] = 7;	hextab["F"] = hextab["f"] = 15

	split (ENVIRON["QUERY_STRING"], qry, "&");
	
	if ( qry[1] == "I")	# Install
	{
		fileName = urlDecode(qry[2]);	
		# Don't install anything that starts with a . or has a / in it
		if (fileName !~ /^\.|\//) 	
		{
			cmd = "rdb_get system.product.title";
			if(( cmd | getline) > 0)
				product_title=$0;
			else
				exit 1;
			close (cmd);
			num = split (fileName, extstr, ".");	
			if( index( product_title, "5908")==0 && num > 0 && extstr[num]=="ipk") 
			{
				printf ("Installing ipk file: %s;\n", fileName);
				cmd = "ipkg-cl install '/opt/Fusion/prldata/"fileName"' >/tmp/flashtoolMsg.txt";
				system (cmd);		
				if ( "rdb_get avis_password" | getline )
				{
					close ( "rdb_get avis_password" );
					print "Restore existing AVIS settings..." >> "/tmp/flashtoolMsg.txt";
					newfile = "/usr/local/cdcs/conf/override.new";
					overrideFile = "/usr/local/cdcs/conf/override.conf"
					system( "rm " newfile" >/dev/null 2>&1");
					while( "cat "overrideFile | getline )
					{
						mun = split( $0, a, ";"); 
						if( mun == 6 && substr( $0, 1, 1)!= "#" )
						{
							if ( "rdb_get "a[1] | getline value )
							{
								print a[1]";"a[2]";"a[3]";"a[4]";"a[5]";"value > newfile;
							}
							else
							{
								print a[1]";"a[2]";"a[3]";"a[4]";"a[5]";"a[6] > newfile;
							}
						}
						else
						{
							print $0 > newfile;
						}				
					}
					close( "newfile" )
					close( "overrideFile" )
					system( "mv "newfile" " overrideFile );											
				}
			}
			else if(  num > 0 && extstr[num]=="cwe") 
			{
				cmd = "UploadModuleFirmwareAuto.sh '/opt/Fusion/prldata/"fileName"' >/tmp/flashtoolMsg.txt 2>&1 &";
				system (cmd);
				close(cmd);
			}
			else
			{
				cmd = "flashtool '/opt/Fusion/prldata/"fileName "' >/tmp/flashtoolMsg.txt &";
				system (cmd);
			}
		}
		else
		{
			printf ("Unable to install %s\n", fileName);
		}
		print "Done" >> "/tmp/flashtoolMsg.txt";
		print ("<script language=Javascript> setTimeout(\"window.location = '/modulePRLupdate.html' \", 1000); </script> ");
	}
	else if ( qry[1] == "D") # Delete
	{
		fileName = urlDecode(qry[2]);
		
		# Don't delete anything that starts with a . or has a / in it
		if (fileName !~ /^\.|\//) 	
		{
		#	printf ("Deleting %s <p>\n", fileName);
			cmd = "rm '/opt/Fusion/prldata/" fileName "'";
			system (cmd);
		}
		else
			printf ("Unable to delete %s\n", fileName);
		print ("<script language=Javascript> setTimeout(\"window.location = '/modulePRLupdate.html'\", 1000); </script> ");	
	}
	
}

function urlDecode (str)
{
	decoded = ""
	i   = 1
	len = length (str)
	while ( i <= len ) 
	{
	    c = substr (str, i, 1)
	    if ( c == "%" ) 
	    {
	    	if ( i+2 <= len ) 
	    	{
				c1 = substr (str, i+1, 1)
				c2 = substr (str, i+2, 1)
				if ( hextab[c1] != "" && hextab[c2] != "" )
				{
			   		code = 0 + hextab[c1] * 16 + hextab[c2] + 0
		    		c = sprintf ("%c", code)
		    		i = i + 2
		    	}
		    }
		} 
	    else if ( c == "+" ) 
	    {	# special handling: "+" means " "
	    	c = " "
	    }
	    decoded = decoded c
	    ++i
	}
	return decoded
}