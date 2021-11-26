###########################################################################
#   Copyright (C) 2004-2010 by Dimark Software Inc.                       #
#   support@dimark.com                                                    #
###########################################################################

1) QUICK-START INSTRUCTIONS FOR COMPILING DIMARK TR-069 CLIENT
ON A Fedora/Centos/RedHat LINUX PLATFORM:

1. Unpack the Dimark client source into its own directory.

2. Unpack gsoap_2.7.6c.tar.gz into a separate directory and 
follow the instructions for running config(ure) and compiling
on the Linux HOST platform.

When completed, copy the binary gsoap_2.7.6c/soapcpp2 to the
gsoap directory (.e.g <dimark_client_src>/gsoap/> under the 
Dimark client source directory.

3. These instructions assume OpenSSL is already installed on
your platform.  If not, obtain any version of OpenSSL from
0.9.7f through 0.9.8i and follow the directions for compiling
and installing.

3. Cd to the Dimark client source directory.  Review the file:

   Makefile

and edit as required.  When ready to compile, type:

   make

The Dimark client will first call the soapcpp2 process to 
compile the gSoap directives to produce the necessary files
to continue compilation. Some warning messages will be 
produced by soapcpp2. These are normal and can be ignored.

The compilation will continue.

When complete the following binary will be present:

   dimclient

NOTE: See the start.sh script for how the client needs
to be started and its target data directory prepared.

Start start.sh

#end
