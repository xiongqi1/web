/*
   GSM-7 packing routing.
*/
int                                /* Returns -1 for success, 0 for failure */
SMS_GSMEncode(
        int             inSize,         /* Number of GSM-7 characters */
        char*           inText,         /* Pointer to the GSM-7 characters. Note: I could
       not have used a 0-terminated string, since 0
       represents '@' in the GSM-7 character set */
        int             paddingBits,    /* If you use a UDH, you may have to add padding
       bits to properly align the GSM-7 septets */
        int             outSize,        /* The number of octets we have available to write */
        unsigned char*  outBuff,        /* A pointer to the available octets */
        int            *outUsed         /* Keeps track of howmany octets actually were used */
)
{
	int             bits = 0;
	int             i;
	unsigned char   octet;
	*outUsed = 0;
	if( paddingBits ) {
		bits = 7 - paddingBits;
		*outBuff++ = inText[0] << (7 - bits);
		(*outUsed) ++;
		bits++;
	}
	for( i = 0; i < inSize; i++ ) {
		if( bits == 7 ) {
			bits = 0;
			continue;
		}
		if( *outUsed == outSize )
			return 0; /* buffer overflow */
		octet = (inText[i] & 0x7f) >> bits;
		if( i < inSize - 1 )
			octet |= inText[i + 1] << (7 - bits);
		*outBuff++ = octet;
		(*outUsed)++;
		bits++;
	}
	return -1; /* ok */
}


/*
GSM-7 packing routing.
*/

int SMS_GSMDecode(char* dst, const unsigned char* src, int septet_len,int padding_bits)
{
	int src_len;
	int dst_len;
	int b;
	unsigned char left;

	src_len = 0;
	dst_len = 0;

	b = 0;
	left = 0;

	while(dst_len<septet_len) {

		if(padding_bits && !src_len) {
			b=7-padding_bits;
		}
		else {
			*dst++ = ((*src << b) | left) & 0x7f;
			dst_len++;
		}

		if(!(dst_len<septet_len))
			break;
		
		left = *src >> (7-b);

		b++;

		if(b == 7) {
			*dst = left;

			dst++;
			dst_len++;

			b = 0;
			left = 0;
		}

		src++;
		src_len++;
	}

	return dst_len;
}
