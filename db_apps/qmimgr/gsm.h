#ifndef __GSM_H__
#define __GSM_H__

int SMS_GSMEncode(
	      int             inSize,         /* Number of GSM-7 characters */
       char*           inText,         /* Pointer to the GSM-7 characters. Note: I could
       not have used a 0-terminated string, since 0
       represents '@' in the GSM-7 character set */
       int             paddingBits,    /* If you use a UDH, you may have to add padding
       bits to properly align the GSM-7 septets */
       int             outSize,        /* The number of octets we have available to write */
       unsigned char*  outBuff,        /* A pointer to the available octets */
       int            *outUsed         /* Keeps track of howmany octets actually were used */
);

int SMS_GSMDecode(char* dst, const unsigned char* src, int septet_len,int padding_bits);

#endif
