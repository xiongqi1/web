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

//#include "logging.h"
#include <syslog.h>
#include <stdio.h>
#include "gsm7.h"


// iso = ISO8859-15 (you might change the table to any other 8-bit character set)
// sms = sms character set used by mobile phones

//                  iso   sms
char charset[] = { '@' , 0x00,
                   0xA3, 0x01,
                   '$' , 0x02,
                   0xA5, 0x03,
                   0xE8, 0x04,
                   0xE9, 0x05,
                   0xF9, 0x06,
                   0xEC, 0x07,
                   0xF2, 0x08,
                   0xC7, 0x09,
                   0x0A, 0x0A,
                   0xD8, 0x0B,
                   0xF8, 0x0C,
                   0x0D, 0x0D,
                   0xC5, 0x0E,
                   0xE5, 0x0F,

// ISO8859-7, Capital greek characters
//         0xC4, 0x10,
//         0x5F, 0x11,
//         0xD6, 0x12,
//         0xC3, 0x13,
//         0xCB, 0x14,
//         0xD9, 0x15,
//         0xD0, 0x16,
//         0xD8, 0x17,
//         0xD3, 0x18,
//         0xC8, 0x19,
//         0xCE, 0x1A,

// ISO8859-1, ISO8859-15
                   0x81, 0x10,
                   0x5F, 0x11,
                   0x82, 0x12,
                   0x83, 0x13,
                   0x84, 0x14,
                   0x85, 0x15,
                   0x86, 0x16,
                   0x87, 0x17,
                   0x88, 0x18,
                   0x89, 0x19,
                   0x8A, 0x1A,

                   0x1B, 0x1B,
                   0xC6, 0x1C,
                   0xE6, 0x1D,
                   0xDF, 0x1E,
                   0xC9, 0x1F,
                   ' ' , 0x20,
                   '!' , 0x21,
                   0x22, 0x22,
                   '#' , 0x23,
                   '%' , 0x25,
                   '&' , 0x26,
                   0x27, 0x27,
                   '(' , 0x28,
                   ')' , 0x29,
                   '*' , 0x2A,
                   '+' , 0x2B,
                   ',' , 0x2C,
                   '-' , 0x2D,
                   '.' , 0x2E,
                   '/' , 0x2F,
                   '0' , 0x30,
                   '1' , 0x31,
                   '2' , 0x32,
                   '3' , 0x33,
                   '4' , 0x34,
                   '5' , 0x35,
                   '6' , 0x36,
                   '7' , 0x37,
                   '8' , 0x38,
                   '9' , 0x39,
                   ':' , 0x3A,
                   ';' , 0x3B,
                   '<' , 0x3C,
                   '=' , 0x3D,
                   '>' , 0x3E,
                   '?' , 0x3F,
                   0xA1, 0x40,
                   'A' , 0x41,
                   'B' , 0x42,
                   'C' , 0x43,
                   'D' , 0x44,
                   'E' , 0x45,
                   'F' , 0x46,
                   'G' , 0x47,
                   'H' , 0x48,
                   'I' , 0x49,
                   'J' , 0x4A,
                   'K' , 0x4B,
                   'L' , 0x4C,
                   'M' , 0x4D,
                   'N' , 0x4E,
                   'O' , 0x4F,
                   'P' , 0x50,
                   'Q' , 0x51,
                   'R' , 0x52,
                   'S' , 0x53,
                   'T' , 0x54,
                   'U' , 0x55,
                   'V' , 0x56,
                   'W' , 0x57,
                   'X' , 0x58,
                   'Y' , 0x59,
                   'Z' , 0x5A,
                   0xC4, 0x5B,
                   0xD6, 0x5C,
                   0xD1, 0x5D,
                   0xDC, 0x5E,
                   0xA7, 0x5F,
                   0xBF, 0x60,
                   'a' , 0x61,
                   'b' , 0x62,
                   'c' , 0x63,
                   'd' , 0x64,
                   'e' , 0x65,
                   'f' , 0x66,
                   'g' , 0x67,
                   'h' , 0x68,
                   'i' , 0x69,
                   'j' , 0x6A,
                   'k' , 0x6B,
                   'l' , 0x6C,
                   'm' , 0x6D,
                   'n' , 0x6E,
                   'o' , 0x6F,
                   'p' , 0x70,
                   'q' , 0x71,
                   'r' , 0x72,
                   's' , 0x73,
                   't' , 0x74,
                   'u' , 0x75,
                   'v' , 0x76,
                   'w' , 0x77,
                   'x' , 0x78,
                   'y' , 0x79,
                   'z' , 0x7A,
                   0xE4, 0x7B,
                   0xF6, 0x7C,
                   0xF1, 0x7D,
                   0xFC, 0x7E,
                   0xE0, 0x7F,
                   0x60, 0x27,
                   0xE1, 0x61,  // replacement for accented a
                   0xED, 0x69,  // replacement for accented i
                   0xF3, 0x6F,  // replacement for accented o
                   0xFA, 0x75,  // replacement for accented u
                   0   , 0     // End marker
                 };

// Extended characters. In GSM they are preceeded by 0x1B.

char ext_charset[] = { 0x0C, 0x0A,
                       '^' , 0x14,
                       '{' , 0x28,
                       '}' , 0x29,
                       '\\', 0x2F,
                       '[' , 0x3C,
                       '~' , 0x3D,
                       ']' , 0x3E,
                       0x7C, 0x40,
                       0xA4, 0x65,
                       0   , 0     // End marker
                     };


int iso2gsm(char* source, int size, char* destination, int max)
{
	int table_row;
	int source_count = 0;
	int dest_count = 0;
	int found = 0;
	destination[dest_count] = 0;
	if (source == 0 || size == 0)
		return 0;
	// Convert each character untl end of string
	while (source_count < size && dest_count < max)
	{
		// search in normal translation table
		found = 0;
		table_row = 0;
		while (charset[table_row*2])
		{
			if (charset[table_row*2] == source[source_count])
			{
				destination[dest_count++] = charset[table_row*2+1];
				found = 1;
				break;
			}
			table_row++;
		}
		// if not found in normal table, then search in the extended table
		if (found == 0)
		{
			table_row = 0;
			while (ext_charset[table_row*2])
			{
				if (ext_charset[table_row*2] == source[source_count])
				{
					destination[dest_count++] = 0x1B;
					if (dest_count < max)
						destination[dest_count++] = ext_charset[table_row*2+1];
					found = 1;
					break;
				}
				table_row++;
			}
		}
		// if also not found in the extended table, then log a note
		if (found == 0)
		{
			fprintf(stderr, "Cannot convert ISO character %c 0x%2X to GSM, you might need to update the translation tables.", source[source_count], source[source_count]);
		}
		source_count++;
	}
	// Terminate destination string with 0, however 0x00 are also allowed within the string.
	destination[dest_count] = 0;
	return dest_count;
}

int gsm2iso(const char* source, int size, char* destination, int max)
{
	int table_row;
	int source_count = 0;
	int dest_count = 0;
	int found = 0;
	if (source == 0 || size == 0)
	{
		destination[0] = 0;
		return 0;
	}
	// Convert each character untl end of string
	while (source_count < size && dest_count < max)
	{
		if (source[source_count] != 0x1B)
		{
			// search in normal translation table
			found = 0;
			table_row = 0;
			while (charset[table_row*2])
			{
				if (charset[table_row*2+1] == source[source_count])
				{
					destination[dest_count++] = charset[table_row*2];
					found = 1;
					break;
				}
				table_row++;
			}
			// if not found in the normal table, then log a note
			if (found == 0)
			{
				fprintf(stderr, "Cannot convert GSM character 0x%2X to ISO, you might need to update the 1st translation table.", source[source_count]);
			}
		}
		else if (++source_count < size)
		{
			// search in extended translation table
			found = 0;
			table_row = 0;
			while (ext_charset[table_row*2])
			{
				if (ext_charset[table_row*2+1] == source[source_count])
				{
					destination[dest_count++] = ext_charset[table_row*2];
					found = 1;
					break;
				}
				table_row++;
			}
			// if not found in the normal table, then log a note
			if (found == 0)
			{
				fprintf(stderr, "Cannot convert extended GSM character 0x1B 0x%2X, you might need to update the 2nd translation table.", source[source_count]);
			}
		}
		source_count++;
	}
	// Terminate destination string with 0, however 0x00 are also allowed within the string.
	destination[dest_count] = 0;
	return dest_count;
}

/***********************************************************************************************************************************/
/***********************************************************************************************************************************/
/***********************************************************************************************************************************/

typedef struct {
    unsigned int unicode;
    char charcode;
} char_enc_type;
//                 unicode   sms
char_enc_type uni_charset[] = {
                    { 0x0040, 0x00},    /* @ : COMMERCIAL AT */
                    { 0x00A3, 0x01},    /* £ : POUND SIGN */
                    { 0x0024, 0x02},    /* $ : DOLLAR SIGN */
                    { 0x00A5, 0x03},    /* ¥ : YEN SIGN */
                    { 0x00E8, 0x04},    /* è : LATIN SMALL LETTER E WITH GRAVE */
                    { 0x00E9, 0x05},    /* é : LATIN SMALL LETTER E WITH ACUTE */
                    { 0x00F9, 0x06},    /* ù : LATIN SMALL LETTER U WITH GRAVE */
                    { 0x00EC, 0x07},    /* ì : LATIN SMALL LETTER I WITH GRAVE */
                    { 0x00F2, 0x08},    /* ò : LATIN SMALL LETTER O WITH GRAVE */
                    { 0x00C7, 0x09},    /* Ç : LATIN CAPITAL LETTER C WITH CEDILLA */
                    { 0x000A, 0x0A},    /* LF */
                    { 0x00D8, 0x0B},    /* Ø : LATIN CAPITAL LETTER O WITH STROKE */
                    { 0x00F8, 0x0C},    /* ø : LATIN SMALL LETTER O WITH STROKE */
                    { 0x000D, 0x0D},    /* CR */
                    { 0x00C5, 0x0E},    /* Å : LATIN CAPITAL LETTER A WITH RING ABOVE */
                    { 0x00E5, 0x0F},    /* å : LATIN SMALL LETTER A WITH RING ABOVE */
                    { 0x0394, 0x10},    /* Δ : GREEK CAPITAL LETTER DELTA */
                    { 0x005F, 0x11},    /* _ : LOW LINE */
                    { 0x03A6, 0x12},    /* Φ : GREEK CAPITAL LETTER PHI */
                    { 0x0393, 0x13},    /* Γ : GREEK CAPITAL LETTER GAMMA */
                    { 0x039B, 0x14},    /* Λ : GREEK CAPITAL LETTER LAMBDA */
                    { 0x03A9, 0x15},    /* Ω : GREEK CAPITAL LETTER OMEGA */
                    { 0x03A0, 0x16},    /* Π : GREEK CAPITAL LETTER PI */
                    { 0x03A8, 0x17},    /* Ψ : GREEK CAPITAL LETTER PSI */
                    { 0x03A3, 0x18},    /* Σ : GREEK CAPITAL LETTER SIGMA */
                    { 0x0398, 0x19},    /* Θ : GREEK CAPITAL LETTER THETA */
                    { 0x039E, 0x1A},    /* Ξ : GREEK CAPITAL LETTER XI */
                    { 0x001B, 0x1B},    /* ESCAPE TO EXTENSION TABLE */
                    { 0x00C6, 0x1C},    /* Æ : LATIN CAPITAL LETTER AE */
                    { 0x00E6, 0x1D},    /* æ : LATIN SMALL LETTER AE */
                    { 0x00DF, 0x1E},    /* ß : LATIN SMALL LETTER SHARP S */
                    { 0x00C9, 0x1F},    /* É : LATIN CAPITAL LETTER E WITH ACUTE */
                    { 0x0020, 0x20},    /* SPACE */
                    { 0x0021, 0x21},    /* ! : EXCLAMATION MARK */
                    { 0x0022, 0x22},    /* " : QUOTATION MARK */
                    { 0x0023, 0x23},    /* # : NUMBER SIGN */
                    { 0x00A4, 0x24},    /* ¤ : CURRENCY SIGN */
                    { 0x0025, 0x25},    /* % : PERCENT SIGN */
                    { 0x0026, 0x26},    /* & : AMPERSAND */
                    { 0x0027, 0x27},    /* ' : APOSTROPHE */
                    { 0x0028, 0x28},    /* ( : LEFT PARENTHESIS */
                    { 0x0029, 0x29},    /* ) : RIGHT PARENTHESIS */
                    { 0x002A, 0x2A},    /* * : ASTERISK */
                    { 0x002B, 0x2B},    /* + : PLUS SIGN */
                    { 0x002C, 0x2C},    /* , : COMMA */
                    { 0x002D, 0x2D},    /* - : HYPHEN-MINUS */
                    { 0x002E, 0x2E},    /* . : FULL STOP */
                    { 0x002F, 0x2F},    /* / : SOLIDUS */
                    { 0x0030, 0x30},    /* 0 : DIGIT ZERO */
                    { 0x0031, 0x31},    /* 1 : DIGIT ONE */
                    { 0x0032, 0x32},    /* 2 : DIGIT TWO */
                    { 0x0033, 0x33},    /* 3 : DIGIT THREE */
                    { 0x0034, 0x34},    /* 4 : DIGIT FOUR */
                    { 0x0035, 0x35},    /* 5 : DIGIT FIVE */
                    { 0x0036, 0x36},    /* 6 : DIGIT SIX */
                    { 0x0037, 0x37},    /* 7 : DIGIT SEVEN */
                    { 0x0038, 0x38},    /* 8 : DIGIT EIGHT */
                    { 0x0039, 0x39},    /* 9 : DIGIT NINE */
                    { 0x003A, 0x3A},    /* : : COLON */
                    { 0x003B, 0x3B},    /* ; : SEMICOLON */
                    { 0x003C, 0x3C},    /* < : LESS-THAN SIGN */
                    { 0x003D, 0x3D},    /* = : EQUALS SIGN */
                    { 0x003E, 0x3E},    /* > : GREATER-THAN SIGN */
                    { 0x003F, 0x3F},    /* ? : QUESTION MARK */
                    { 0x00A1, 0x40},    /* ¡ : INVERTED EXCLAMATION MARK */
                    { 0x0041, 0x41},    /* A : LATIN CAPITAL LETTER A */
                    { 0x0042, 0x42},    /* B : LATIN CAPITAL LETTER B */
                    { 0x0043, 0x43},    /* C : LATIN CAPITAL LETTER C */
                    { 0x0044, 0x44},    /* D : LATIN CAPITAL LETTER D */
                    { 0x0045, 0x45},    /* E : LATIN CAPITAL LETTER E */
                    { 0x0046, 0x46},    /* F : LATIN CAPITAL LETTER F */
                    { 0x0047, 0x47},    /* G : LATIN CAPITAL LETTER G */
                    { 0x0048, 0x48},    /* H : LATIN CAPITAL LETTER H */
                    { 0x0049, 0x49},    /* I : LATIN CAPITAL LETTER I */
                    { 0x004A, 0x4A},    /* J : LATIN CAPITAL LETTER J */
                    { 0x004B, 0x4B},    /* K : LATIN CAPITAL LETTER K */
                    { 0x004C, 0x4C},    /* L : LATIN CAPITAL LETTER L */
                    { 0x004D, 0x4D},    /* M : LATIN CAPITAL LETTER M */
                    { 0x004E, 0x4E},    /* N : LATIN CAPITAL LETTER N */
                    { 0x004F, 0x4F},    /* O : LATIN CAPITAL LETTER O */
                    { 0x0050, 0x50},    /* P : LATIN CAPITAL LETTER P */
                    { 0x0051, 0x51},    /* Q : LATIN CAPITAL LETTER Q */
                    { 0x0052, 0x52},    /* R : LATIN CAPITAL LETTER R */
                    { 0x0053, 0x53},    /* S : LATIN CAPITAL LETTER S */
                    { 0x0054, 0x54},    /* T : LATIN CAPITAL LETTER T */
                    { 0x0055, 0x55},    /* U : LATIN CAPITAL LETTER U */
                    { 0x0056, 0x56},    /* V : LATIN CAPITAL LETTER V */
                    { 0x0057, 0x57},    /* W : LATIN CAPITAL LETTER W */
                    { 0x0058, 0x58},    /* X : LATIN CAPITAL LETTER X */
                    { 0x0059, 0x59},    /* Y : LATIN CAPITAL LETTER Y */
                    { 0x005A, 0x5A},    /* Z : LATIN CAPITAL LETTER Z */
                    { 0x00C4, 0x5B},    /* Ä : LATIN CAPITAL LETTER A WITH DIAERESIS */
                    { 0x00D6, 0x5C},    /* Ö : LATIN CAPITAL LETTER O WITH DIAERESIS */
                    { 0x00D1, 0x5D},    /* Ñ : LATIN CAPITAL LETTER N WITH TILDE */
                    { 0x00DC, 0x5E},    /* Ü : LATIN CAPITAL LETTER U WITH DIAERESIS */
                    { 0x00A7, 0x5F},    /* § : SECTION SIGN */
                    { 0x00BF, 0x60},    /* ¿ : INVERTED QUESTION MARK */
                    { 0x0061, 0x61},    /* a : LATIN SMALL LETTER A */
                    { 0x0062, 0x62},    /* b : LATIN SMALL LETTER B */
                    { 0x0063, 0x63},    /* c : LATIN SMALL LETTER C */
                    { 0x0064, 0x64},    /* d : LATIN SMALL LETTER D */
                    { 0x0065, 0x65},    /* e : LATIN SMALL LETTER E */
                    { 0x0066, 0x66},    /* f : LATIN SMALL LETTER F */
                    { 0x0067, 0x67},    /* g : LATIN SMALL LETTER G */
                    { 0x0068, 0x68},    /* h : LATIN SMALL LETTER H */
                    { 0x0069, 0x69},    /* i : LATIN SMALL LETTER I */
                    { 0x006A, 0x6A},    /* j : LATIN SMALL LETTER J */
                    { 0x006B, 0x6B},    /* k : LATIN SMALL LETTER K */
                    { 0x006C, 0x6C},    /* l : LATIN SMALL LETTER L */
                    { 0x006D, 0x6D},    /* m : LATIN SMALL LETTER M */
                    { 0x006E, 0x6E},    /* n : LATIN SMALL LETTER N */
                    { 0x006F, 0x6F},    /* o : LATIN SMALL LETTER O */
                    { 0x0070, 0x70},    /* p : LATIN SMALL LETTER P */
                    { 0x0071, 0x71},    /* q : LATIN SMALL LETTER Q */
                    { 0x0072, 0x72},    /* r : LATIN SMALL LETTER R */
                    { 0x0073, 0x73},    /* s : LATIN SMALL LETTER S */
                    { 0x0074, 0x74},    /* t : LATIN SMALL LETTER T */
                    { 0x0075, 0x75},    /* u : LATIN SMALL LETTER U */
                    { 0x0076, 0x76},    /* v : LATIN SMALL LETTER V */
                    { 0x0077, 0x77},    /* w : LATIN SMALL LETTER W */
                    { 0x0078, 0x78},    /* x : LATIN SMALL LETTER X */
                    { 0x0079, 0x79},    /* y : LATIN SMALL LETTER Y */
                    { 0x007A, 0x7A},    /* z : LATIN SMALL LETTER Z */
                    { 0x00E4, 0x7B},    /* ä : LATIN SMALL LETTER A WITH DIAERESIS */
                    { 0x00F6, 0x7C},    /* ö : LATIN SMALL LETTER O WITH DIAERESIS */
                    { 0x00F1, 0x7D},    /* ñ : LATIN SMALL LETTER N WITH TILDE */
                    { 0x00FC, 0x7E},    /* ü : LATIN SMALL LETTER U WITH DIAERESIS */
                    { 0x00E0, 0x7F},    /* à : LATIN SMALL LETTER A WITH GRAVE */
                    { 0   , 0   }  // End marker
                 };

// Extended characters. In GSM they are preceeded by 0x1B.
//                 unicode   sms
char_enc_type ext_uni_charset[] = {
                    { 0x000C, 0x0A},    /* FORM FEED */
                    { 0x005E, 0x14},    /* ^ : CIRCUMFLEX ACCENT */
                    { 0x007B, 0x28},    /* { : LEFT CURLY BRACKET */
                    { 0x007D, 0x29},    /* } : RIGHT CURLY BRACKET */
                    { 0x005C, 0x2F},    /* \ : REVERSE SOLIDUS (BACKSLASH) */
                    { 0x005B, 0x3C},    /* [ : LEFT SQUARE BRACKET */
                    { 0x007E, 0x3D},    /* ~ : TILDE */
                    { 0x005D, 0x3E},    /* ] : RIGHT SQUARE BRACKET */
                    { 0x007C, 0x40},    /* | : VERTICAL LINE */
                    { 0x20AC, 0x65},    /* € : EURO SIGN */
                    {  0   , 0  }   // End marker
                 };
                     
int unicode2gsm(unsigned int* source, int size, char* destination, int max)
{
    int table_row;
    int source_count = 0;
    int dest_count = 0;
    int found = 0;
    destination[dest_count] = 0;
    if (source == 0 || size == 0)
        return 0;
    // Convert each character untl end of string
    while (source_count < size && dest_count < max)
    {
        // search in normal translation table
        found = 0;
        table_row = 0;
        while (uni_charset[table_row].unicode)
        {
            if (uni_charset[table_row].unicode == source[source_count])
            {
                destination[dest_count++] = uni_charset[table_row].charcode;
                found = 1;
                break;
            }
            table_row++;
        }
        // if not found in normal table, then search in the extended table
        if (found == 0)
        {
            table_row = 0;
            while (ext_uni_charset[table_row].unicode)
            {
                if (ext_uni_charset[table_row].unicode == source[source_count])
                {
                    destination[dest_count++] = 0x1B;
                    if (dest_count < max)
                        destination[dest_count++] = ext_uni_charset[table_row].charcode;
                    found = 1;
                    break;
                }
                table_row++;
            }
        }
        // if also not found in the extended table, then log a note
        if (found == 0)
        {
            fprintf(stderr, "Cannot convert ISO character %c 0x%2X to GSM, you might need to update the translation tables.", source[source_count], source[source_count]);
        }
        source_count++;
    }
    // Terminate destination string with 0, however 0x00 are also allowed within the string.
    destination[dest_count] = 0;
    return dest_count;
}

int gsm2unicode(const char* source, int size, unsigned int* destination, int max)
{
	int table_row;
	int source_count = 0;
	int dest_count = 0;
	int found = 0;
	if (source == 0 || size == 0)
	{
		destination[0] = 0;
		return 0;
	}
	// Convert each character untl end of string
	while (source_count < size && dest_count < max)
	{
		if (source[source_count] != 0x1B)
		{
			// search in normal translation table
			found = 0;
			table_row = 0;
			while (uni_charset[table_row].unicode)
			{
				if (uni_charset[table_row].charcode == source[source_count])
				{
					destination[dest_count++] = uni_charset[table_row].unicode;
					found = 1;
					break;
				}
				table_row++;
			}
			// if not found in the normal table, then log a note
			if (found == 0)
			{
				fprintf(stderr, "Cannot convert GSM character 0x%2X to Unicode, you might need to update the 1st translation table.", source[source_count]);
			}
		}
		else if (++source_count < size)
		{
			// search in extended translation table
			found = 0;
			table_row = 0;
			while (ext_uni_charset[table_row].unicode)
			{
				if (ext_uni_charset[table_row].charcode == source[source_count])
				{
					destination[dest_count++] = ext_uni_charset[table_row].unicode;
					found = 1;
					break;
				}
				table_row++;
			}
			// if not found in the normal table, then log a note
			if (found == 0)
			{
				fprintf(stderr, "Cannot convert extended GSM character 0x1B 0x%2X, you might need to update the 2nd translation table.", source[source_count]);
			}
		}
		source_count++;
	}
	// Terminate destination string with 0, however 0x00 are also allowed within the string.
	destination[dest_count] = 0;
	return dest_count;
}
