#! /usr/bin/awk -f 
BEGIN 
{
	print ( "Content-type: text/html\n" );
	cmd = "cat /tmp/emergency.txt 2>/dev/null";
	cmd | getline;
	close( cmd );
	print "trigger='"$0"';\n"
}
