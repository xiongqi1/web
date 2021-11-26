#ifndef __SIMACCESS_H__
#define __SIMACCESS_H__

int read_simfile_record(int fileid,int recordno,char* buf,int buflen);
int read_simfile_binary(int fileid,char* buf,int buflen);


#endif