// Copyright (c) 2020 Casa Systems
//
// Generating variant.js

let variantShFile = process.argv[2];
let variantJsonFile = process.argv[3];

if (!variantShFile || !variantJsonFile) {
    console.error("Insufficient parameters");
    console.log("Usage: nodejs genVariantJson.js <path to variant.sh> <path to output variant.json");
    process.exit(1);
}

let fs = require("fs");

if (!fs.existsSync(variantShFile)) {
    console.error("%s do not exist", variantShFile);
    process.exit(1);
}

let variant: any = {};
fs.readFileSync(variantShFile, 'utf-8').split("\n").forEach( (line: string) => {
    let match = line.match(/^(V_.+)='(.*)'$/);
    if (match) {
        variant[match[1]] = match[2];
    }
});

fs.writeFile(variantJsonFile, JSON.stringify(variant), (error: any) => {
    if (error) {
        console.error("Failed to write file %s", variantJsonFile);
        process.exit(1);
    }
});
