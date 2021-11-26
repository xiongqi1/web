#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <signal.h>

#include <syslog.h>

#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

void getword(char* word, int maxlen, char* line, char stop)
{
  int x, y;

  for (y = x = 0;line[x] != '\0' && line[x] != stop;x++)
  {
    if (y < maxlen)
    {
      word[y++] = line[x];
    }
  }

  word[y] = '\0';

  if (line[x] != '\0')
    x++;

  y = 0;

  /* copy rest of line */
  while (1)
  {
    if ((line[y] = line[x]) == '\0')
      break;
    x++;
    y++;
  }
}

void getwordamp(char* word, int maxlen, char* line)
{
  int prev_stop = -1;
  int ind, i;

  for (ind = 0;line[ind] != '\0';ind++)
  {
    if (line[ind] == '&')
    {
      prev_stop = ind;
    }
    else if (line[ind] == '=')
    {
      if (prev_stop != -1)
      {
        ind = prev_stop;
        break;
      }
    }
  }
  maxlen--;
  if (maxlen > ind)
    maxlen = ind;
  strncpy(word, line, maxlen);
  word[maxlen] = '\0';
  i = 0;
  if (line[ind] != '\0')
    ind++;
  while (1)
  {
    if ((line[i++] = line[ind++]) == '\0')
      break;
  }
}

void plustospace(char* str)
{
  for (;* str != '\0';str++)
  {
    if (*str == '+')
      *str = ' ';
  }
}

static int unhex(char c)
{
  return (c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c - 'a' + 10);
}

void unescape(char* s)
{
  char* p;

  p = s;
  while (1)
  {
    if ((*p = * s++) == '%')
    {
      if (*s != '\0')
      {
        *p = unhex(*s) << 4;
        s++;
        if (*s != '\0')
        {
          *p += unhex(*s);
          s++;
        }
      }
    }
    if (*p == '\0')
      break;
    p++;
  }
}

char* quotable(char* outquote, int maxlen, char* inquote)
{
  int i;
  i = 0;
  while (*inquote != '\0')
  {
    switch (*inquote)
    {
      case '"':
      case '\\':
        if (i >= maxlen - 3)
          break;
        outquote[i++] = '\\';
        outquote[i++] = * inquote++;
        continue;
      default:
        if (i >= maxlen - 2)
          break;
        outquote[i++] = * inquote++;
        continue;
    }
  }
  outquote[i] = '\0';
  return outquote;
}

void getvariable(char* var, int maxlen, char* line, char stop)
{
  // if (stop == '&')
  //  getwordamp(var, maxlen, line);
  // else
  getword(var, maxlen, line, stop);
  plustospace(var);
  unescape(var);
}

char* urlEscape(char* dest, unsigned maxlen, char* str)
{
  unsigned i, j;
  unsigned x;
  for (j = i = 0;str[i] != '\0';i++)
  {
    if (j >= maxlen - 1)
    {
      dest[j] = '\0';
      return dest;
    }
    if (isdigit(str[i]) || isalpha(str[i]))
    {
      dest[j++] = str[i];
      continue;
    }
    if (str[i] == ' ')
    {
      dest[j++] = '+';
      continue;
    }
    if (j >= maxlen - 3)
    {
      dest[j] = '\0';
      return dest;
    }
    dest[j++] = '%';
    x = ((str[i] >> 4) & 0xf);
    x = x > 10 ? 'a' + x - 10 : '0' + x;
    dest[j++] = x;
    x = ((str[i]) & 0xf);
    x = x > 10 ? 'a' + x - 10 : '0' + x;
    dest[j++] = x;
  }
  dest[j++] = '\0';
  return dest;
}

int create_path(char* name, int mode)
{
  char hold[128];
  char* old_ptr;
  char* ptr;
  int oldmask;


  strncpy(hold, name, sizeof(hold) - 1);
  hold[sizeof(hold)-1] = '\0';
  old_ptr = hold;
  if (hold[0] == '\0')
    return 1;
  if (hold[0] == '/')
  {
    old_ptr++;
    if (hold[1] == '\0')
      return 1;
  }
  oldmask = umask(0);
  do
  {
    ptr = strchr(old_ptr, '/');
    old_ptr = ptr;
    if (ptr)
    {
      *ptr = '\0';
      old_ptr++;
    }
    // printf("Attempt mkdir %s\n", hold);
    if (mkdir(hold, mode) == -1)
    {
      if (errno != EEXIST)
      {
        umask(oldmask);
        return 0;
      }
    }
    if (ptr)
    {
      *ptr = '/';
    }
  }
  while (old_ptr);
  umask(oldmask);
  return 1;
}

char* intrim(char* str)
{
  int slen;
  int i;
  slen = strlen(str);
  while (--slen >= 0)
  {
    if (isspace(str[slen]) || str[slen] == '\"')
    {
      str[slen] = '\0';
      continue;
    }
    break;
  }
  for (slen = 0;str[slen];slen++)
  {
    if (isspace(str[slen]) || str[slen] == '\"')
    {
      continue;
    }
    break;
  }
  if (slen)
  {
    for (i = 0;str[slen+i] != '\0';i++)
      str[i] = str[slen+i];
    str[i] = '\0';
  }
  return str;
}

void getwordquotes(char* word, int maxlen, char* line, char stop)
{
  int x, y;

  int inquote = 0;
  for (y = x = 0;line[x] != '\0' && (inquote || line[x] != stop);x++)
  {
    if (line[x] == '"')
      inquote ^= 1;
    if (y < maxlen)
    {
      word[y++] = line[x];
    }
  }

  word[y] = '\0';

  if (line[x] != '\0')
    x++;

  y = 0;

  /* copy rest of line */
  while (1)
  {
    if ((line[y] = line[x]) == '\0')
      break;
    x++;
    y++;
  }
}
