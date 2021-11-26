#ifndef _GLOBAL_H
#define _GLOBAL_H

#ifdef _DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#ifdef USE_VFORK
#define FORK vfork
#else
#define FORK fork
#endif

#define DEFAULT_SCRIPT "/usr/lib/rdbinit/rdbinit.lua"

#endif
