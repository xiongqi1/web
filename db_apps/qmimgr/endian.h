#define _endian_get_byte(x,i)	((unsigned char*)(&(x)))[i]
#define _endian_set_byte(d,i,v)	{ ((unsigned char*)(&(d)))[i]=(unsigned char)(v); } while(0)

#define read16_from_little_endian(x)	( (unsigned short)( (_endian_get_byte(x,0)) | (_endian_get_byte(x,1)<<8) ) )
#define write16_to_little_endian(x,d)	{ unsigned short t=(x); _endian_set_byte(d,0,t & 0xff); _endian_set_byte(d,1,(t>>8) & 0xff); } while(0)

#define read32_from_little_endian(x)	( (unsigned short)( (_endian_get_byte(x,0)) | (_endian_get_byte(x,1)<<8) | (_endian_get_byte(x,2)<<16) ) | (_endian_get_byte(x,3)<<24) )
#define write32_to_little_endian(x,d)	{ unsigned int t=(x); _endian_set_byte(d,0,t & 0xff); _endian_set_byte(d,1,(t>>8) & 0xff); _endian_set_byte(d,2,(t>>16) & 0xff); _endian_set_byte(d,3,(t>>24) & 0xff);} while(0)
