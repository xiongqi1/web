// This file pulls together all the files required by the Web compiler
// This file is joined with the page definition JS and the Web compiler
// and then run through the C preprocessor

import * as fs from "fs"; // This is for file operations
import * as util from "util" // This is for inclusion of util module for functions such as util.format

// There are dummy functions and variables
declare function startPagePolls(): void;
declare function stopPagePolls(): void;
declare function requestSinglePageObject(pgeObj: PageObject): void;
declare function sendObjects(fnSuccess?: any, fnFail?: any): void;
declare function sendSingleObject(pageObj?: PageObject, fnSuccess?: any, fnFail?: any): void;
declare function sendTheseObjects(objs: any, fnSuccess?: any, fnFail?: any): void;
declare function isFormValid(): boolean;
declare function waitUntilRebootComplete(estRebootingSecs: number, defLanIpAddr?: string): void;
declare function genRebootWaitingMsgHtml(): string;
declare function sendRdbCommand(rdbPrefix: string, cmd: string, param: any, timeout: number,  csrfToken: string, fnSuccess?: any): void;
declare function saveOrRestoreSettings(mode:string, password:string, csrfToken:string, fnSuccess?: any): void;

declare var process: any;
var filesUploadable: any;
var loginStatus: any;

interface Window {
    clipboardData: any;
}

// This array of strings allows extra scripts to be appended to
// the generated html template by calling addExtraScript()
var extraScripts = [];

#include "src/common/legacyJsTypes.ts"
#include "src/common/ntc_utils.ts"
#include "src/common/ntc_html.ts"
#include "builder/luaScriptGen.ts"
#include "src/common/ntc_table.ts"
#include "src/common/pageElements.ts"
#include "src/common/pageObjects.ts"
#include "src/common/themeGenTypes.ts"

// After here is the page defintion and the Web compiler
// Make sure there is a blank line after this
