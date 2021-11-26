//This is international translation function
// param str is the phrase to be translated
// retval is the translated string or str if no tranlation is possible
function _(str){
    if (isDefined(xlat)){
        var newStr=xlat[str];
        if(newStr) {
            newStr =  newStr.replace(/&#x2F;/g, '/');
            newStr =  newStr.replace(/&amp;/g, '&');
            return newStr;
        }
    }
    return str;
}

function encode(data) {return Base64.encode(data);}
function decode(data) {return Base64.decode(data);}

function toBool(b) {
    if (typeof b === "string")
        return b !== '0' && b !== '';
    if (typeof b === "number")
        return b !== 0;
    if (typeof b === "boolean")
        return b;
    return false;
}

function validateEmail(email) {
    // As can be seen from https://stackoverflow.com/questions/46155/how-to-validate-email-address-in-javascript
    // proper email validation is not trivial
    // The following commented lines where the best answer there
/*
    var re = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
    return re.test(email);
*/
    // but we'll go even simpler and just look for a @
    return email.indexOf("@") > 0;
}

