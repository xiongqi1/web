// This file pulls together all the files required by the Web compiler
// This file is joined with the page definition JS and the Web compiler
// and then run through the C preprocessor

import * as fs from "fs"; // This is for file operations
import * as util from "util" // This is for inclusion of util module for functions such as util.format

// There are dummy functions and variables
declare function startPagePolls(): void;
declare function sendObjects(fnSuccess?: any, fnFail?: any): void;
declare function sendTheseObjects(objs: any, fnSuccess?: any, fnFail?: any): void;
declare function isFormValid(): boolean;
declare var process: any;
var filesUploadable: any;

interface Window {
    clipboardData: any;
}

// This array of strings allows extra scripts to be appended to
// the generated html template by calling addExtraScript()
var extraScripts = [];

#include "legacyJsTypes.ts"
#include "ntc_utils.ts"
#include "ntc_html.ts"
#include "../luaScriptGen.js"
#include "ntc_table.ts"
#include "pageElements.ts"
#include "pageObjects.ts"

// After here is the page defintion and the Web compiler
// Make sure there is a blank line after this
