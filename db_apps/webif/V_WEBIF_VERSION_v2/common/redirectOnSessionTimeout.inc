#if 0
//Common include to check the session is current and if not redirect to the
//login page, then after logging in redirect back to the referring page.
//
//Note a redirect on a non-root user can be effected by defining the
//pre-processor symbol "CHECK_ROOT_USER" before including this file.
//
//This should be included using the C preprocessor after the
//topinc_v2.inc include as it has the following dependencies:
//- util.js:HostDate()
//- jQuery
#endif

<!-- Common include to check the session is current -->
<%
#if 0
// Check the session, if timed out redirect, and pass the referring page as
// an argument. This allows index.html to redirect back to the referring page.
#endif
#define CHECK_SESSION_NO_CODE redirect('/index.html?src=' + request["SCRIPT_NAME"]);
#include "checkSession.inc"
#undef CHECK_SESSION_NO_CODE
%>
