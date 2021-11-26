/*
 * NetComm OMA-DM Client
 *
 * lua_wbxml.c
 * Lua wrapper library for LibWBXML.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdbool.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <wbxml.h>
#include <wbxml_parser.h>

#include <lextras.h>

#include "lua_wbxml.h"

#define MODULE_NAME "wbxml"

/* Called after the document header is parsed. Create a
 * table to represent the document and store any useful
 * information in it, leaving the table on the top of
 * the stack. */
static void __wbxml_start_doc(void *ctx, WBXMLCharsetMIBEnum charset, const WBXMLLangEntry *lang)
{
    lua_State* L = ctx;

    lua_createtable(L, 0, 6);
    lua_pushinteger(L, charset);
    lua_setfield(L, -2, "charset");
    lua_pushinteger(L, lang->langID);
    lua_setfield(L, -2, "langID");
    lua_pushinteger(L, lang->publicID->wbxmlPublicID);
    lua_setfield(L, -2, "wbxmlID");
    lua_pushstring(L, lang->publicID->xmlPublicID);
    lua_setfield(L, -2, "xmlID");
    lua_pushstring(L, lang->publicID->xmlDTD);
    lua_setfield(L, -2, "xmlDTD");
}

/* Called when a new element tag has been parsed. Create
 * a table to represent the element, store the element's
 * name and attributes, then leave the table on top of
 * the stack. */
static void __wbxml_start_element(void *ctx, WBXMLTag *name, WBXMLAttribute **attrs)
{
    lua_State* L = ctx;

    lua_createtable(L, 0, 2);
    lua_pushstring(L, (const char*)wbxml_tag_get_xml_name(name));
    lua_setfield(L, -2, "name");
    lua_newtable(L);
    for (; *attrs; attrs++) {
        lua_pushstring(L, (const char*)wbxml_attribute_get_xml_value(*attrs));
        lua_setfield(L, -2, (const char*)wbxml_attribute_get_xml_name(*attrs));
    }
    lua_setfield(L, -2, "attributes");
}

/* Called when an element end tag has been parsed. At this
 * point the element table is on top of the stack, with its
 * parent element or document directly underneath it. Store
 * the element in it's parent, popping it from the stack. */
static void __wbxml_end_element(void *ctx, WBXMLTag *name)
{
    (void)name;

    lua_State* L = ctx;
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
}

/* Called when character data is parsed. At this point the
 * table for the enclosing element is on top of the stack,
 * so we store the character data in that element, appending
 * it to any character data that is already there. */
static void __wbxml_characters(void *ctx, WB_UTINY *cdata, WB_ULONG start, WB_ULONG length)
{
    lua_State* L = ctx;

    lua_getfield(L, -1, "cdata");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushlstring(L, (const char*)cdata + start, length);
    } else {
        lua_pushlstring(L, (const char*)cdata + start, length);
        lua_concat(L, 2);
    }
    lua_setfield(L, -2, "cdata");
}

/* Attempt to parse the supplied WBXML blob. If successful,
 * returns a single table containing the parsed document
 * tree. */
static int __decode(lua_State* L, const char* str, int len, int lang)
{
    WBXMLContentHandler handlers = {
        .start_document_clb = __wbxml_start_doc,
        .end_document_clb   = NULL,
        .start_element_clb  = __wbxml_start_element,
        .end_element_clb    = __wbxml_end_element,
        .characters_clb     = __wbxml_characters,
        .pi_clb             = NULL,
    };

    WBXMLParser* wb = wbxml_parser_create();
    if (!wb) {
        luaL_error(L, "wbxml_parser_create() failed.");
    }

    wbxml_parser_set_content_handler(wb, &handlers);
    wbxml_parser_set_user_data(wb, L);
    if (lang != WBXML_LANG_UNKNOWN) {
        wbxml_parser_set_language(wb, lang);
    }

    int rc = wbxml_parser_parse(wb, (WB_UTINY*)str, len);
    if (rc) {
        wbxml_parser_destroy(wb);
        luaL_error(L, "wbxml_parser_parse() failed: %s.", wbxml_errors_string(rc));
    }

    wbxml_parser_destroy(wb);
    return 1;
}

/* wbxml.Decode(data)
 *
 * Attempt to parse a chunk of WBXML and return
 * the resulting document tree if successful.
 */
static int __wbxml_decode(lua_State* L)
{
    size_t len;
    const char* ptr = luaL_checklstring(L, 1, &len);

    return __decode(L, ptr, len, WBXML_LANG_UNKNOWN);
}

/* wbxml.DecodeAs(lang, data)
 *
 * Attempt to parse a chunk of WBXML as the specified
 * language, ignoring any embedded public ID. Return
 * the resulting document tree if successful.
 */
static int __wbxml_decodeAs(lua_State* L)
{
    size_t len;
    int lang = luaL_checkinteger(L, 1);
    const char* str = luaL_checklstring(L, 2, &len);

    return __decode(L, str, len, lang);
}

/* Load library into Lua environment. */
void luaopen_wbxml(lua_State* L)
{
    /* Library functions. */
    static const luaL_Reg funcs[] = {
        {"Decode",   __wbxml_decode},
        {"DecodeAs", __wbxml_decodeAs},
        {NULL,     NULL}
    };

    /* Constants mapped from LibWBXML. */
    static const luaL_Enum enums[] = {
        {"CHARSET_UNKNOWN",         WBXML_CHARSET_UNKNOWN},
        {"CHARSET_US_ASCII",        WBXML_CHARSET_US_ASCII},
        {"CHARSET_ISO_8859_1",      WBXML_CHARSET_ISO_8859_1},
        {"CHARSET_ISO_8859_2",      WBXML_CHARSET_ISO_8859_2},
        {"CHARSET_ISO_8859_3",      WBXML_CHARSET_ISO_8859_3},
        {"CHARSET_ISO_8859_4",      WBXML_CHARSET_ISO_8859_4},
        {"CHARSET_ISO_8859_5",      WBXML_CHARSET_ISO_8859_5},
        {"CHARSET_ISO_8859_6",      WBXML_CHARSET_ISO_8859_6},
        {"CHARSET_ISO_8859_7",      WBXML_CHARSET_ISO_8859_7},
        {"CHARSET_ISO_8859_8",      WBXML_CHARSET_ISO_8859_8},
        {"CHARSET_ISO_8859_9",      WBXML_CHARSET_ISO_8859_9},
        {"CHARSET_SHIFT_JIS",       WBXML_CHARSET_SHIFT_JIS},
        {"CHARSET_UTF_8",           WBXML_CHARSET_UTF_8},
        {"CHARSET_ISO_10646_UCS_2", WBXML_CHARSET_ISO_10646_UCS_2},
        {"CHARSET_UTF_16",          WBXML_CHARSET_UTF_16},
        {"CHARSET_BIG5",            WBXML_CHARSET_BIG5},
        {"LANG_UNKNOWN",            WBXML_LANG_UNKNOWN},
    #if defined(WBXML_SUPPORT_WML)
        {"LANG_WML10",              WBXML_LANG_WML10},
        {"LANG_WML11",              WBXML_LANG_WML11},
        {"LANG_WML12",              WBXML_LANG_WML12},
        {"LANG_WML13",              WBXML_LANG_WML13},
    #endif
    #if defined(WBXML_SUPPORT_WTA)
        {"LANG_WTA10",              WBXML_LANG_WTA10},
        {"LANG_WTAWML12",           WBXML_LANG_WTAWML12},
        {"LANG_CHANNEL11",          WBXML_LANG_CHANNEL11},
        {"LANG_CHANNEL12",          WBXML_LANG_CHANNEL12},
    #endif
    #if defined(WBXML_SUPPORT_SI)
        {"LANG_SI10",               WBXML_LANG_SI10},
    #endif
    #if defined(WBXML_SUPPORT_SL)
        {"LANG_SL10",               WBXML_LANG_SL10},
    #endif
    #if defined(WBXML_SUPPORT_CO)
        {"LANG_CO10",               WBXML_LANG_CO10},
    #endif
    #if defined(WBXML_SUPPORT_PROV)
        {"LANG_PROV10",             WBXML_LANG_PROV10},
    #endif
    #if defined(WBXML_SUPPORT_EMN)
        {"LANG_EMN10",              WBXML_LANG_EMN10},
    #endif
    #if defined(WBXML_SUPPORT_DRMREL)
        {"LANG_DRMREL10",           WBXML_LANG_DRMREL10},
    #endif
    #if defined(WBXML_SUPPORT_OTA_SETTINGS)
        {"LANG_OTA_SETTINGS",       WBXML_LANG_OTA_SETTINGS},
    #endif
    #if defined(WBXML_SUPPORT_SYNCML)
        {"LANG_SYNCML_SYNCML10",    WBXML_LANG_SYNCML_SYNCML10},
        {"LANG_SYNCML_DEVINF10",    WBXML_LANG_SYNCML_DEVINF10},
        {"LANG_SYNCML_METINF10",    WBXML_LANG_SYNCML_METINF10},
        {"LANG_SYNCML_SYNCML11",    WBXML_LANG_SYNCML_SYNCML11},
        {"LANG_SYNCML_DEVINF11",    WBXML_LANG_SYNCML_DEVINF11},
        {"LANG_SYNCML_METINF11",    WBXML_LANG_SYNCML_METINF11},
        {"LANG_SYNCML_SYNCML12",    WBXML_LANG_SYNCML_SYNCML12},
        {"LANG_SYNCML_DEVINF12",    WBXML_LANG_SYNCML_DEVINF12},
        {"LANG_SYNCML_METINF12",    WBXML_LANG_SYNCML_METINF12},
        {"LANG_SYNCML_DMDDF12",     WBXML_LANG_SYNCML_DMDDF12},
    #endif
    #if defined(WBXML_SUPPORT_WV)
        {"LANG_WV_CSP11",           WBXML_LANG_WV_CSP11},
        {"LANG_WV_CSP12",           WBXML_LANG_WV_CSP12},
    #endif
    #if defined(WBXML_SUPPORT_AIRSYNC)
        {"LANG_AIRSYNC",            WBXML_LANG_AIRSYNC},
        {"LANG_ACTIVESYNC",         WBXML_LANG_ACTIVESYNC},
    #endif
    #if defined(WBXML_SUPPORT_CONML)
        {"LANG_CONML",              WBXML_LANG_CONML},
    #endif
        {NULL, 0}
    };

    /* Create library and install all functions in it. */
    luaL_register(L, MODULE_NAME, funcs);
    luaL_setenums(L, enums);
    lua_pop(L, 1);
}
