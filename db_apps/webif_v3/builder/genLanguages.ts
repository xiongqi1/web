// Copyright (c) 2020 Casa Systems
//
// Generating international translation js files
// Usage: nodejs genLanguages.js path-to-languages-directory path-to-variant.json path-to-output-dir

let langDir = process.argv[2];
let variantJsonFile = process.argv[3];
let outputDir = process.argv[4];

if (!langDir || !variantJsonFile || !outputDir) {
    console.error("Insufficient parameters");
    console.log("Usage: nodejs genLanguages.js path-to-languages-directory path-to-variant.json path-to-output-dir");
    process.exit(1);
}

if (!langDir.endsWith("/")) {
    langDir = langDir + "/";
}

if (!outputDir.endsWith("/")) {
    outputDir = outputDir + "/";
}

let fs = require("fs");

if (!fs.existsSync(langDir) || !fs.existsSync(variantJsonFile) || !fs.existsSync(outputDir)) {
    console.error("%s, %s, or %s do not exist", langDir, variantJsonFile, outputDir);
    process.exit(1);
}

let variant = JSON.parse(fs.readFileSync(variantJsonFile));

if (!variant["V_LANGUAGES"]) {
    console.log("V_LANGUAGES is not defined, processing English only");
    variant["V_LANGUAGES"] = "en";
}

// process entry files in given directory dirPath
function processDir(dirPath: string, stringMap: { [x: string]: any; }) {
    console.log("Processing directory %s", dirPath);
    if (fs.existsSync(dirPath)) {
        fs.readdirSync(dirPath).forEach((dEntry: string) => {
            let filePath = dirPath + dEntry;
            if (fs.lstatSync(filePath).isFile() && dEntry.endsWith(".json")) {
                let stringsObj = JSON.parse(fs.readFileSync(filePath));
                Object.keys(stringsObj).forEach( key => {
                    stringMap[key] = stringsObj[key];
                });
            }
        });
    }
}

// consider to process a V_VAR name
function processVvar(dir: string, name: string, stringMap: {}) {
    let variantValue = variant[name];
    if (variantValue) {
        let vDirPath = dir + "_VARIANTS/" + name + "/" + variantValue + "/";
        if (fs.existsSync(vDirPath)) {
            processDir(vDirPath, stringMap);
        }
    }
}

variant["V_LANGUAGES"].split(" ").forEach( (lang: string) => {
    let langObj = {};
    let inLangDir = langDir + lang + "/";

    processDir(inLangDir, langObj);

    // processing _VARIANTS
    if (fs.existsSync(inLangDir + "_VARIANTS/")) {
        // first, V_WEBIF_SPEC
        processVvar(inLangDir, "V_WEBIF_SPEC", langObj);
        // other V_vars
        let lastVs: any = {
            "V_SKIN": true,
            "V_CUSTOM_FEATURE_PACK": true,
            "V_PRODUCT": true
        }
        fs.readdirSync(inLangDir + "_VARIANTS/").sort().forEach((dEntry: string) => {
            let fullPath = inLangDir + "_VARIANTS/" + dEntry;
            if (fs.lstatSync(fullPath).isDirectory() && dEntry.startsWith("V_")
                    && !lastVs[dEntry] && dEntry != "V_WEBIF_SPEC") {
                processVvar(inLangDir, dEntry, langObj);
            }
        });
        Object.keys(lastVs).forEach( name => {
            processVvar(inLangDir, name, langObj);
        });
    }

    // write output js file
    if (!fs.existsSync(outputDir + lang)) {
        fs.mkdirSync(outputDir + lang);
    }
    let outputJsFile = outputDir + lang + "/xlat.js";
    var json = JSON.stringify(langObj);
    fs.writeFile(outputJsFile, "var xlat = " + json, (error: any) => {
        if (error) {
            console.error("Failed to write %s", outputJsFile);
            process.exit(1);
        }
    });
});
