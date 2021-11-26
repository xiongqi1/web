//
// Part of GEM - executable manager, support for running templates.
// Code from template manager ported into GEM
// Therefore, kept very much as it is, including formatting and comments
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <syslog.h>

#ifdef V_HOST
#include "../../cdcs_libs/mock_rdb_lib/rdb_operations.h"
#else
#include "rdb_ops.h"
#endif

#include "gem_api.h"
#include "gem_daemon.h"

#define TEMPLATEMGR_DELIMITER_REPLACE_OPEN			"?<"
#define TEMPLATEMGR_DELIMITER_SUBSCRIBE_OPEN		"!<"
#define TEMPLATEMGR_DELIMITER_CLOSE					">;"

typedef enum
{
    variabletype_none,
    variabletype_replace,
    variabletype_subscribe
} variabletype;

///////////////////////////////////////////////////////////////////////////////
static int get_variable(const char* pParserPtr, char* pVariable, variabletype* pType, int* pStartIdx, const char** ppNext)
{
    variabletype variableType = variabletype_none;

    int fBracket = 0;
    int fPrevBracket = 0;
    int cbBracket = strlen(TEMPLATEMGR_DELIMITER_REPLACE_OPEN);
    int iStartIdx = -1;

    const char* pPtr = pParserPtr;

    while (*pPtr)
    {
        if (!fBracket)
        {
            if (!strncmp(pPtr, TEMPLATEMGR_DELIMITER_REPLACE_OPEN, cbBracket))
                variableType = variabletype_replace;
            else if (!strncmp(pPtr, TEMPLATEMGR_DELIMITER_SUBSCRIBE_OPEN, cbBracket))
                variableType = variabletype_subscribe;

            fBracket = variableType != variabletype_none;
        }
        else
        {
            if (!strncmp(pPtr, TEMPLATEMGR_DELIMITER_CLOSE, cbBracket))
                fBracket = 0;
        }

        int fIn = !fPrevBracket && fBracket;
        int fOut = fPrevBracket && !fBracket;
        fPrevBracket = fBracket;

        if (fIn)
            iStartIdx = pPtr - pParserPtr;

        if (fBracket && !(fOut || fIn))
            *pVariable++ = *pPtr;

        if (fOut || fIn)
            pPtr += cbBracket;
        else
            pPtr++;

        if (fOut)
            break;
    }

    *pType = variableType;
    *pVariable = 0;
    *pStartIdx = iStartIdx;
    *ppNext = pPtr;

    if (fBracket)
        return -1;

    if (variableType == variabletype_none)
        return 0;

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
int gem_templatemgr_parse_template(char *template_file_name, FILE* streamOut, int ee_num)
{
    int len;
    FILE* streamIn = fopen(template_file_name, "rt");

    // open input
    streamIn = fopen(template_file_name, "rt");
    if (!streamIn)
    {
        gem_syslog(LOG_CRIT, "failed to open %s for reading", template_file_name);
        return -1;
    }

    int iLine = 0;
    int cVariables = 0;

    char achLineBuf[1024];
    while (fgets(achLineBuf, sizeof(achLineBuf), streamIn) != NULL)
    {
        iLine++;

        const char* pParsePtr = NULL;
        char achVariable[MAX_NAME_LENGTH];
        variabletype variableType;
        int iStartIdx;
        const char* pNextParsePtr = achLineBuf;

        while (1)
        {
            pParsePtr = pNextParsePtr;
            int stat = get_variable(pParsePtr, achVariable, &variableType, &iStartIdx, &pNextParsePtr);

            if (stat == 0)
            {
                break;
            }
            else if (stat < 0)
            {
                gem_syslog(LOG_ERR, "bracket error found at line %d in file %s", iLine, template_file_name);
                break;
            }

            // read-only mode - subscribe to all RDB variables mentioned in the template
            if (!streamOut)
            {
                if (add_rdb_trig_table_elem(achVariable, 0, NULL, ee_num) < 0)
                {
                    gem_syslog(LOG_ERR, "bracket error found at line %d in file %s", iLine, template_file_name);
                    break;
                }

                gem_syslog(LOG_DEBUG, "%s variable (type:%d) added from line %d in file %s",
                           achVariable, variableType, iLine, template_file_name);
                cVariables++;

                continue;
            }

            // build mode - replace or remove
            char achValue[sizeof(achLineBuf)];
            if (fwrite(pParsePtr, sizeof(char), iStartIdx, streamOut) != iStartIdx)
            {
                gem_syslog(LOG_CRIT, "disk write failure occured while writing a configuration file");
                continue;
            }

            len = sizeof(achValue);
            if (rdb_get(g_rdb_session, achVariable, achValue, &len) < 0)
            {
                gem_syslog(LOG_CRIT, "database error occured while putting variable %s at line %d in template %s",
                           achVariable, iLine, template_file_name);
                continue;
            }

            if (variableType == variabletype_subscribe)
            {
                // remove the whole line
                if (*pNextParsePtr == '\n' && iStartIdx == 0)
                {
                    pParsePtr = NULL;
                    break;
                }
            }
            else
            {
                fputs(achValue, streamOut);
                //gem_syslog(LOG_DEBUG, "variable %s(%s) stored at line %d in template %s",
                //    achVariable, achValue, iLine, template_file_name);
            }
        }

        if (streamOut && pParsePtr)
            fputs(pParsePtr, streamOut);
    }

    if (!streamOut)
    {
        gem_syslog(LOG_DEBUG, "%d line(s) of %s file parsed for subscribing %d variable(s) ", iLine, template_file_name, cVariables);
    }

    if (streamIn)
        fclose(streamIn);

    return 0;
}
