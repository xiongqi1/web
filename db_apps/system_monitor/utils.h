#ifndef _UTILS_H
#define _UTILS_H

/* Replaces the filename in a path string. Strings must be large enough. */
const char *filename(char *path, int path_end, const char *fname);

/* Returns the first (up to) 50 characters of a file. Not re-entrant. */
const char *readfile(const char *fname);

/* Reads and returns natural (positive+0) number from specified file.
Returns -1 & perror in case of error.
*/
int read_natural(const char *fname);

#endif /* _UTILS_H */
