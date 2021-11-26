var usernameLabel = "username";
var passwordLabel = "password";
var csrfTokenLabel = "csrfToken";
var userNameUI = editableTextVariable("username","User Name", {class: "border1"});
var userPasswordUI = editablePasswordVariable("password", "User Password", {class: "border1"});

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

var LoginInput = PageObj("login","Login",
 {
    extraAttr: {
      headerTag: "h2-login",    // h2 header tag name
      labelTag: "label-login",  // login label tag name
    },
    decodeRdb: function(obj) {
        var accountsNames = obj.loginAccounts.split(",");
        obj.username = accountsNames[0];
        if (obj.username == '') {
            obj.username = 'root';
        }
        return obj;
    },
    members: [
        userNameUI,
        userPasswordUI,
        buttonAction("submit","Login","onLoginSubmit","", {buttonStyle: "loginButton"}),
        hiddenVariable("loginAccounts", "admin.user.accounts")
    ]
 });


var pageData : PageDef = {
#ifndef V_WEBIF_SERVER_turbo
  onDevice: false, // This page needs further work for Appweb. The logon button does a POST to /login which does not yet exist!!
#endif
  title: "Login",
  noTitleBox: true,
  menuPos: ["Login", ""],
  authenticatedOnly: false,
  validateOnload: false,
  pageObjects: [LoginInput],
  alertSuccessTxt: "",
  style: { bodyCssClass: "body-box-index" },
  onReady: function (){
    // hide logoff icon in this page
    setLogoffVisible(false);
    //TODO: keydown event supporting in safari,ie and opera is unknown per MSN api
    //https://developer.mozilla.org/en-US/docs/Web/API/Document/keydown_event
    document.addEventListener("keydown", function(keyEvent){
        if (keyEvent.key === "Enter"){
            onLoginSubmit()
        }
    });
  }
}
