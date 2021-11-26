<%
// This file defines some security functions:
// - Protection against CSRF
// - Frame-breaker script to reject loading Web UI in embbeded frames

// ================= Begin - Protection against CSRF ===============
csrfTokenName = "csrfToken";
csrfTokenName_for = csrfTokenName+"_for";
csrfTokenValidity = "csrfTokenValidity";
csrfTokenInvalid = "invalid";
csrfTokenNameForGet = csrfTokenName+"Get";
csrfTokenNameForPost = csrfTokenName+"Post";
csrfInvalidTokenRedirectLink = request["REQUEST_URI"];
if (request["QUERY_STRING"] == undefined || request["QUERY_STRING"] == "") {
	csrfInvalidTokenRedirectLink = csrfInvalidTokenRedirectLink + "?" + csrfTokenValidity+ "=" + csrfTokenInvalid;
}
else {
	if(form[csrfTokenValidity] != csrfTokenInvalid){
		// Avoid repeated "csrfTokenValidity=invalid" in QUERY_STRING
		csrfInvalidTokenRedirectLink = csrfInvalidTokenRedirectLink + "&" + csrfTokenValidity+ "=" + csrfTokenInvalid;
}
}

// 1) Checking CSRF token in POST request. If token in the request is invalid, stop processing the request.
// 2) Generating new token
function checkCsrfToken() {
	if (request['REQUEST_METHOD'] == "POST") {
		if (session[csrfTokenName_for] == undefined || session[csrfTokenName] == undefined || form[csrfTokenNameForPost] == undefined
			|| session[csrfTokenName] == "" || form[csrfTokenNameForPost] == ""
			|| session[csrfTokenName_for] != request["SCRIPT_NAME"] || session[csrfTokenName] != form[csrfTokenNameForPost]) {
			redirect(csrfInvalidTokenRedirectLink);
			exit(-1);
		}
	}
	// generate new token
	session[csrfTokenName] = generate_random_token(request["SESSION_ID"]);
	session[csrfTokenName_for] = request["SCRIPT_NAME"];
}

// write CSRF token into a hidden field to append it into a form
function appendCsrfToken() {
%>
<input type="hidden" name="@@csrfTokenNameForPost" value="@@session[csrfTokenName]">
<%
}

// write CSRF token into query string
function appendCsrfTokenToQueryString() {
	write("&"+csrfTokenNameForGet+"="+session[csrfTokenName]+"&");
}

// Supporting mechanism for protection against CSRF on document-ready: indication of invalid token
function supportCsrfTokenOnDocumentReady() {
	if(form[csrfTokenValidity]==csrfTokenInvalid) {
%>
<script type="text/javascript">
$(document).ready(function(){
	alertInvalidCsrfToken();
});
</script>
<%
		request['QUERY_STRING'] = trim_string(request['QUERY_STRING'], "&"+csrfTokenValidity+"="+csrfTokenInvalid);
		request['QUERY_STRING'] = trim_string(request['QUERY_STRING'], csrfTokenValidity+"="+csrfTokenInvalid);
	}
}

// Supporting indication of invalid token status from CGI script
function supportCsrfTokenErrorCgi() {
%>
<script type="text/javascript">
var unloadingPage = 0;
$(document).ajaxError(function( event, jqxhr, settings, thrownError ) {
	if (jqxhr.status == 498 && unloadingPage == 0) {
		window.location = "@@csrfInvalidTokenRedirectLink";
	}
});
window.onbeforeunload = function(){
	unloadingPage = 1;
}
window.onunload = function(){
	unloadingPage = 1;
}
</script>
<%
}

// ============== End - Protection against CSRF ================

// frame-breaker: The page is covered at the begining, then if javascript can run and the page
// is not inside a frame, the cover is removed. 
function frameBreaker() {
%>
<style id="coveredWhileChecking">body{display:none !important;}</style>
<script type="text/javascript">
if (self === top) {
        var coveredWhileChecking = document.getElementById("coveredWhileChecking");
        coveredWhileChecking.parentNode.removeChild(coveredWhileChecking);
} else {
        top.location = self.location;
}
</script>
<%
}
%>
