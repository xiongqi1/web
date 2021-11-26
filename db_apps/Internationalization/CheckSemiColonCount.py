#!/usr/bin/python
#
# Findout missing semicolon in string lines
# Currently each string line should have 14 semicolons.
#
# Command line usage: python CheckSemiColonCount.py
#
import sys

def checkSemiColonCount():
    # read the old strings file
    with open("./strings.csv") as strings_file:
        old_lines = strings_file.readlines()
        
    print "count :    string"
    print "------------------------------------------------------------------------------"
    for line in old_lines:
        strings_file_fields = line.split(';')
        if len(strings_file_fields) != 14:
			print len(strings_file_fields), "   :   ", line

# Do the job!
checkSemiColonCount()
