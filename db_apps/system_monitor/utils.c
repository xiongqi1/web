/*
Utilities to access system information, e.g. sysfs, proc, etc.
*/
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

/* Replaces the filename in a path string. Strings must be large enough. */
const char *filename(char *path, int path_end, const char *fname)
{
	strcpy(path+path_end, fname);
	return path;
}

/* Returns the first (up to) 50 characters of a file. Not re-entrant. */
const char *readfile(const char *fname)
{
	#define MAXFILE 50
	static char buffer[MAXFILE+1];
	int fd;
	ssize_t bytes;
	fd=open(fname,O_RDONLY);
	if (fd<0) return NULL;
	bytes=read(fd, buffer, MAXFILE);
	close(fd);
	if (bytes<0) return NULL;
	buffer[bytes]='\0';
	return buffer;
}


/* Reads and returns natural (positive+0) number from specified file.
Returns -1 & perror in case of error.
*/
int read_natural(const char *fname)
{
	const char *b;
	b=readfile(fname);
	if (!b) return -1;
	return atoi(b);
}
