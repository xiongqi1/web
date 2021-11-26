// Copyright (c) 2020 Casa Systems
//
// Generating entries list
// Usage: nodejs genEntriesDef.js path-to-entries_def path-to-variant.json path-to-output-js-file

let entriesDir = process.argv[2];
let variantJsonFile = process.argv[3];
let outputJsFile = process.argv[4];

if (!entriesDir || !variantJsonFile || !outputJsFile) {
    console.error("Insufficient parameters");
    console.log("Usage: nodejs genEntriesDef.js path-to-entries_def path-to-variant.json path-to-output-js-file");
    process.exit(1);
}

if (!entriesDir.endsWith("/")) {
    entriesDir = entriesDir + "/";
}

let fs = require("fs");

if (!fs.existsSync(entriesDir) || !fs.existsSync(variantJsonFile)) {
    console.error("%s or %s do not exist", entriesDir, variantJsonFile);
    process.exit(1);
}

const variant = JSON.parse(fs.readFileSync(variantJsonFile));
let entryMap: any[] = [];

// find an entry for given id in provided entryList
// @retval index in given list or null if not found
function findEntry(entryList: any[], id: any) {
    for (let i = 0; i < entryList.length; i++) {
        if (entryList[i].id == id) {
            return i;
        }
    }
    return null;
}

// process input entry file filePath, placing entries in appropriate parent list in global entryMap
function processJsonEntryFile(filePath: string) {
    const entryDefJson = fs.readFileSync(filePath);
    const entryDef = JSON.parse(entryDefJson);

    if (!entryDef["id"] || !entryDef["title"]) {
        console.error("Entry definition is insufficient");
        console.error(entryDef);
        process.exit(1);
    }

    // find right entry list (right children entry list if it has parent)
    let eList = entryMap;

    if (typeof entryDef["path"] !== "undefined" && Array.isArray(entryDef.path) && entryDef.path.length > 0) {
        //find parent from root
        let entriesLevel = entryMap;
        entryDef.path.forEach( (parentId: any) => {
            let pEntryPos = findEntry(entriesLevel, parentId);
            if (pEntryPos == null) {
                pEntryPos = entriesLevel.length;
                // add placeholder for parent
                entriesLevel[pEntryPos] = {
                    id: parentId,
                    children: []
                };
            }
            entriesLevel = entriesLevel[pEntryPos].children;
        });
        eList = entriesLevel;
    }

    // set entry
    let entryPos = findEntry(eList, entryDef.id);
    if (entryPos == null) {
        // new entry
        entryPos = eList.length;
        eList[entryPos] = {
            id: entryDef["id"],
            title: entryDef["title"],
            url: entryDef["url"],
            position: entryDef["position"] ? entryDef["position"] : 0,
            viewGroups: entryDef["viewGroups"] ? entryDef["viewGroups"] : ["root", "admin"],
            children: []
        };
    } else {
        // update existing entry
        eList[entryPos].title = entryDef["title"];
        eList[entryPos].position = entryDef["position"];
        eList[entryPos].url = entryDef["url"];
        // children should already exist
    }
}

// process entry files in given directory dirPath
function processDir(dirPath: string) {
    console.log("Processing directory %s", dirPath);
    fs.readdirSync(dirPath).forEach((dEntry: string) => {
        let fullPath = dirPath + dEntry;
        if (fs.lstatSync(fullPath).isFile() && dEntry.endsWith(".json")) {
            processJsonEntryFile(fullPath);
        }
    });
}

// consider to process a V_VAR name
function processVvar(name: string) {
    const variantValue = variant[name];
    if (variantValue) {
        let vDirPath = entriesDir + "_VARIANTS/" + name + "/" + variantValue + "/";
        if (fs.existsSync(vDirPath)) {
            processDir(vDirPath);
        }
    }
}

// position ascending sort on given entry list
function sortPos(a: any, b: any) {
    return a.position - b.position;
}
function sortEntryMap(entryList: any[]) {
    entryList.sort(sortPos);
    for (let i = 0; i < entryList.length; i++) {
        if (entryList[i].children.length > 0) {
            sortEntryMap(entryList[i].children);
        }
    }
}

// processing main entries directory
processDir(entriesDir);

// processing _VARIANTS
// first, V_WEBIF_SPEC
processVvar("V_WEBIF_SPEC");
// other V_vars
let lastVs: any = {
    "V_SKIN": true,
    "V_CUSTOM_FEATURE_PACK": true,
    "V_PRODUCT": true
}
fs.readdirSync(entriesDir + "_VARIANTS/").sort().forEach((dEntry: string) => {
    let fullPath = entriesDir + "_VARIANTS/" + dEntry;
    if (fs.lstatSync(fullPath).isDirectory() && dEntry.startsWith("V_")
            && !lastVs[dEntry] && dEntry != "V_WEBIF_SPEC") {
        processVvar(dEntry);
    }
});
Object.keys(lastVs).forEach( name => {
    processVvar(name);
});

// sort entries by position
sortEntryMap(entryMap);

// write output js file
const json = JSON.stringify(entryMap);
fs.writeFile(outputJsFile, "var MENU_ENTRIES = " + json, (error: any) => {
    if (error) {
        console.error("Failed to write %s", outputJsFile);
        process.exit(1);
    }
});
