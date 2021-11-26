#!/usr/bin/python
#
# Incomplete tool done to patch a particular Japanese translation into strings.csv
#
# This tool is similar to merge_spec_scv.awk which patches newly translated strings (located in another csv file)
# to the strings file. At the moment, hard-coded for Japanese strings, also assumes that translations file is
# in the format of:
# Token;Spec;English;Japanese
#
# This can be used as a basis for a future proper tool to do these merges.
# Note: if the input translations file has CR LF terminations, it needs to be manually changed to LF-s
# 
# Command line usage: python PatchStrings.py "translations file name", e.g.:
# python PatchStrings.py string_spec_ntc_v2.csv
#
import sys

def patchStrings(translationsFileName):
    # read the old strings file
    with open("./strings.csv") as strings_file:
        old_lines = strings_file.readlines()

    # @TODO - use timestamp instead
    backup_file = open("./strings2.csv", "w")
    print "Name of the backup file: ", backup_file.name

    backup_file.writelines(old_lines)
    backup_file.close()

    # read translations file name is passed as the only command line argument
    with open(translationsFileName) as translations_file:
        translated_lines = translations_file.readlines()
    translations_file.close()

    # write all replaced tokens to log.txt
    log_file = open("./log.txt", "w")
    log_file.write("This file contains an auto-generated list of replaced tokens\n")
    log_file.write("============================================================\n")

    match_count = 0
    new_lines = [] # will build patched lines
    for line in old_lines:
        strings_file_fields = line.split(';')
        for t_line in translated_lines:
            # @TODO - this end of line replacement still doesn't do what it is meant to do
            t_line = t_line.replace("\r\n", "\n") # translations file seems to have \r\n terminations which stuffs things up
            # format - token, spec, english, japanese
            token, spec, eng, jpn = t_line.split(';')
            # fail safe - do not patch if something looks wrong
            if token != None and token == strings_file_fields[0] and jpn != "" and jpn != None and len(strings_file_fields) >= 13:
                #print "will replace Id ", strings_file_fields[0], " text ", strings_file_fields[-1], " with new text ", jpn
                # replace Japanese string
                strings_file_fields[-1] = jpn
                # re-joing the line
                line = ";".join(strings_file_fields)
                log_file.write(token + "\n")
                match_count = match_count + 1
                # no point looking any further as tokens are unique in translations file, so bail
                break
        new_lines.append(line) # just copy the old line (possibly unmodified)

    strings_file = open("./strings.csv", "w")
    strings_file.writelines(new_lines)
    strings_file.close()
    log_file.close()
    print "Patched ", match_count, " lines"


# Execution starts here
# Get the total number of args passed to the demo.py
if len(sys.argv) != 2:
    print "Error, please give exactly one argument being the name of the translations csv file"
    sys.exit()

# Do the job!
patchStrings(sys.argv[1])
