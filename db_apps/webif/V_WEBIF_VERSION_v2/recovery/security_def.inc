<%
// This file defines some security functions:
// - Protection against CSRF

// ================= Begin - Protection against CSRF ===============
csrfTokenName = "csrfToken";
csrfTokenNameForGet = csrfTokenName+"Get";

// Generating new token
session[csrfTokenName] = generate_random_token(request["SESSION_ID"]);

// write CSRF token into query string
function appendCsrfTokenToQueryString() {
	write("&"+csrfTokenNameForGet+"="+session[csrfTokenName]+"&");
}

// ============== End - Protection against CSRF ================
%>
