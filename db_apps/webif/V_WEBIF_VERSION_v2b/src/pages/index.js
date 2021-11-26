var usernameLabel = "username";
var passwordLabel = "password";
var csrfTokenLabel = "csrfToken";
var userNameUI = editableTextVariable("username","User Name");
var userPasswordUI = editablePasswordVariable("password", "User Password");

function onLoginSubmit()
{
    if (isFormValid()){
        var userInfo = {};
        userInfo[usernameLabel] = userNameUI.getVal();
        userInfo[passwordLabel] = encode(userPasswordUI.getVal());
        userInfo[csrfTokenLabel] = csrfToken;
        if (userInfo[usernameLabel]  && userInfo[passwordLabel]){
                var postLogin = $.post("login",
                            JSON.stringify(userInfo),
                            function(response){
                                console.log("post sucessful"+response);
                                if (response.result === 0) { //login sucessed
                                    success_alert("Login", "Welcome");
                                    if(response.url){
                                        window.location.assign(response.url);
                                        //redirect to early url before login
                                    }else {
                                        window.location.assign(window.location.hostname+"status.html")
                                        //this is default page after login, or server assign one for it
                                    }
                                } else if (response.result === 2) {  //login retry time limit reached
                                    console.log("retry time limit reached");
                                } else {
                                    validate_alert("Login", _("admin passWarning")); //lookup xlat table to check proper message
                                }
                            },
                            "json")
                    .fail(function( xhr, textStatus, errorThrown) {
                        console.log("login post request failed", xhr.responseText);
                    });
        }
     }
}

var LoginInput = PageObj("login","User Login",
 {
   members: [
        userNameUI,
        userPasswordUI,
        buttonAction("submit","Submit","onLoginSubmit","")
   ]
 });


var pageData : PageDef = {
#ifndef V_WEBIF_SERVER_turbo
       onDevice: false, // This page needs further work for Appweb. The logon button does a POST to /login which does not yet exist!!
#endif
        title: "Login",
        menuPos: ["Login", ""],
        authenticatedOnly: false,
        validateOnload: false,
        pageObjects: [LoginInput],
        alertSuccessTxt: "",
        onReady: function (){
                userNameUI.setVal(null, "root");
                //TODO: keydown event supporting in safari,ie and opera is unknown per MSN api
                //https://developer.mozilla.org/en-US/docs/Web/API/Document/keydown_event
                document.addEventListener("keydown", function(keyEvent){
                    if (keyEvent.key === "Enter"){
                        onLoginSubmit()
                    }
                });
        }
}
