/*
SMS Server Tools
   
Copyright (C) Stefan Frings 

This program is free software unless you got it under another license directly
from the author. You can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation.
Either version 2 of the License, or (at your option) any later version.

http://www.meinemullemaus.de/
mailto: smstools@meinemullemaus.de
*/

#ifndef GSM7_H_20120830
#define GSM7_H_20120830

// Both functions return the size of the converted string
// max limits the number of characters to be written into
// destination
// size is the size of the source string
// max is the maximum size of the destination string
// The GSM character set contains 0x00 as a valid character

int gsm2iso(const char* source, int size, char* destination, int max);
int iso2gsm(char* source, int size, char* destination, int max);
int unicode2gsm(unsigned int* source, int size, char* destination, int max);
int gsm2unicode(const char* source, int size, unsigned int* destination, int max);

#endif  /* GSM7_H_20120830 */
