/*
    webapp.js
    Written by Dalton Messmer <messmer.dalton@gmail.com>.

    Event handlers and helper functions used by the dmf2mod web application
*/

/*global Module, FS*/
const app = document.getElementById("dmf2mod_app");
const appFieldset = document.getElementById("dmf2mod_app_fieldset");
const statusArea = document.getElementById("status_area");

let appInitialised = false;
let errorMessage = "";
let warningMessage = "";
let statusMessageIsError = true;

Module["onRuntimeInitialized"] = function () {
    console.log("Loaded dmf2mod");
}

Module["postRun"] = function () {
    console.log("Initialized dmf2mod");
    loadOptions();
    app.style.display = "block";
    document.getElementById("loading").style.display = "none";
    appInitialised = true;
}

function setStatusMessage() {
    statusArea.innerHTML = "";
    if (errorMessage.length > 0) {
        errorMessage = errorMessage.replace("\n", "<br>");
        statusArea.innerHTML += "<div style='color: red;'>" + errorMessage + "</div>";
        errorMessage = "";
    }
    if (warningMessage.length > 0) {
        warningMessage = warningMessage.replace("\n", "<br>");
        statusArea.innerHTML += "<div style='color: black;'>" + warningMessage + "</div>";
        warningMessage = "";
    }
}

function disableControls(disable) {
    // TODO: This makes everything appear disabled/enabled, but clicking the Convert button
    //  multiple times before the convertFile function returns will still make it convert the
    //  file multiple times. It's as if the button is never disabled. I've tried for hours to 
    //  fix it and cannot do so.
    appFieldset.disabled = disable;
}

function loadOptions() {
    let optionsElement = document.getElementById("options");
    optionsElement.innerHTML = "<label><b>Output type:</b></label>";

    const availMods = Module.getAvailableModules();
    if (availMods.size() === 0) {
        optionsElement.innerHTML += "<br>ERROR: No supported modules";
        return;
    }

    // Add available modules to convert to
    for (let i = 0; i < availMods.size(); i++) {
        const mtype = availMods.get(i);
        const m = Module.getExtensionFromType(mtype);

        // Add radio button
        let typesHTML = "<input type='radio' id='" + m + "' name='output_type' value='" + m + "'";
        if (i == availMods.size() - 1) {
            typesHTML += " checked"; // Last radio button is checked
        }
        typesHTML += " onchange='onOutputTypeChange(this)'><label for='" + m + "'>" + m + "</label>";

        optionsElement.innerHTML += typesHTML;
    }

    // Add module-specific command-line options
    for (let i = 0; i < availMods.size(); i++) {
        const mtype = availMods.get(i);
        const m = Module.getExtensionFromType(mtype);
        let optionsHTML = "<div id='div_" + m + "' class='module_options' style='display:";
        if (i == availMods.size() - 1) {
            optionsHTML += " block;'>";
        } else {
            optionsHTML += " none;'>";
        }

        const optionDefs = Module.getOptionDefinitions(mtype);
        if (optionDefs.size() === 0) {
            optionsElement.innerHTML += optionsHTML + "<br>(No options)</div>";
            continue;
        }

        optionsHTML += "<br><label><b>Options:</b></label><br>";

        for (let j = 0; j < optionDefs.size(); j++) {
            const o = optionDefs.get(j);

            if (o.acceptedValues.size() === 0) {
                switch (o.valueType) {
                    case Module.OptionValueType.BOOL:
                        const defaultIsChecked = o.defaultValue === "true" ? true : false;
                        optionsHTML += o.displayName + " " + getSliderHTML(m, o.name, defaultIsChecked);
                        break;
                    case Module.OptionValueType.INT:
                        optionsHTML += o.displayName + " " + getNumberHTML(m, o.name, true, o.defaultValue);
                        break;
                    case Module.OptionValueType.DOUBLE:
                        optionsHTML += o.displayName + " " + getNumberHTML(m, o.name, false, o.defaultValue);
                        break;
                    case Module.OptionValueType.STRING:
                        optionsHTML += o.displayName + " " + getTextboxHTML(m, o.name, o.defaultValue);
                        break;
                    default:
                        optionsElement.innerHTML += optionsHTML + "Error loading options</div>";
                        continue;
                }
            } else {
                optionsHTML += o.displayName + " " + getDropdownHTML(m, o.name, o.acceptedValues, o.defaultValue);
            }

            optionsHTML += "<br>";
        }
        optionsElement.innerHTML += optionsHTML + "</div>";
    }
}

function getSliderHTML(id, optionName, checked) {
    let html = "<input type='checkbox' id='" + id + "_" + optionName + "' class='" + id + "_" + "option' name='" + optionName + "'";
    if (checked) {
        html += " checked";
    }
    html += ">";
    return html;
}

function getNumberHTML(id, optionName, isInteger, defaultValue) {
    let html = "<input type='number' id='" + id + "_" + optionName + "' class='" + id + "_" + "option' name='" + optionName + "'";
    if (isInteger) {
        html += " step='1'";
    }
    html += " value='" + defaultValue + "'>";
    return html;
}

function getTextboxHTML(id, optionName, defaultValue) {
    return "<input type='text' id='" + id + "_" + optionName + "' class='" + id + "_" + "option' name='" + optionName + "' value='" + defaultValue + "'>";
}

function getDropdownHTML(id, optionName, dropdownOptions, defaultValue) {
    let html = "<select id='" + id + "_" + optionName + "' class='" + id + "_" + "option' name='" + optionName + "'>";
    for (let i = 0; i < dropdownOptions.size(); i++) {
        const option = dropdownOptions.get(i);
        html += "<option value='" + option + "'";
        if (option === defaultValue) {
            html += " selected";
        }
        html += ">" + option + "</option>";
    }
    html += "</select>";
    return html;
}

function onOutputTypeChange(elem) {
    // Hide command-line options for all modules
    let elements = document.getElementsByClassName("module_options");
    for (const element of elements) {
        element.style.display = "none";
    }

    // Show command-line options for the current module
    const optionsId = "div_" + elem.value;
    document.getElementById(optionsId).style.display = "block";
}

function getOutputType() {
    return document.querySelector("input[name='output_type']:checked").value;
}

function getOptions() {
    const currentOutputModule = getOutputType();
    const optionsForCurrentModule = document.getElementsByClassName(currentOutputModule  + "_option");

    let vo = new Module.VectorOption();

    const len = optionsForCurrentModule.length;
    for (let i = 0; i < len; i++) {
        const option = optionsForCurrentModule[i];

        if (option.nodeName.toLowerCase() == "input" && option.type == "checkbox") {
            vo.push_back([option.name, option.checked ? "true" : "false"]);
        } else {
            vo.push_back([option.name, option.value]);
        }
    }
    return vo;
}

async function importFileLocal(externalFile, internalFilename) {
    if (!appInitialised) {
        return true;
    }

    // Convert blob to Uint8Array (more abstract: ArrayBufferView)
    let data = new Uint8Array(await externalFile.arrayBuffer());

    // Store the file
    let stream = FS.open(internalFilename, "w+");
    FS.write(stream, data, 0, data.length, 0);
    FS.close(stream);

    return false;
}

// From: https://stackoverflow.com/questions/63959571/how-do-i-pass-a-file-blob-from-javascript-to-emscripten-webassembly-c
async function importFileOnline(url, internalFilename) {
    if (!appInitialised) {
        return true;
    }

    // Download a file
    let blob;
    try {
        blob = await fetch(url).then(response => {
            if (!response.ok) {
                return true;
            }
            return response.blob();
        });
    } catch (error) {
        console.log("importFileOnline: " + error);
        return true;
    }

    if (!blob) {
        console.log("importFileOnline: Bad response.");
        return true;
    }

    // Convert blob to Uint8Array (more abstract: ArrayBufferView)
    let data = new Uint8Array(await blob.arrayBuffer());

    // Store the file
    let stream = FS.open(internalFilename, "w+");
    FS.write(stream, data, 0, data.length, 0);
    FS.close(stream);

    return false;
}

async function convertFile() {
    disableControls(true);

    errorMessage = "";
    warningMessage = "";
    setStatusMessage();

    // Check if input file was provided
    const inputFileElem = document.getElementById("input_file");
    if (inputFileElem.value.length === 0) {
        disableControls(false);
        return true;
    }

    // Get input file and input filename to be used internally
    const externalFile = inputFileElem.files.item(0);
    const internalFilenameInput = externalFile.name;

    // Change file extension to get internal filename for output from dmf2mod
    const outputType = getOutputType();
    const options = getOptions();
    let internalFilenameOutput = internalFilenameInput.replace(/\.[^/.]+$/, "") + "." + outputType;

    if (internalFilenameInput == internalFilenameOutput) {
        // No conversion needed: Converting to same type
        warningMessage = "Same module type; No conversion needed";
        setStatusMessage();
        disableControls(false);
        return true;
    }

    let resp = await importFileLocal(externalFile, internalFilenameInput);
    if (resp) {
        disableControls(false);
        return true;
    }

    resp = Module.moduleImport(internalFilenameInput);
    setStatusMessage();
    if (resp) {
        disableControls(false);
        return true;
    }

    let stream = FS.open(internalFilenameOutput, "w+");
    FS.close(stream);

    const result = Module.moduleConvert(internalFilenameOutput, options);
    setStatusMessage();
    if (result) {
        disableControls(false);
        return true;
    }

    const byteArray = FS.readFile(internalFilenameOutput);
    const blob = new Blob([byteArray]);

    let a = document.createElement("a");
    a.download = internalFilenameOutput;
    a.href = URL.createObjectURL(blob);
    a.click();

    disableControls(false);
}

app.addEventListener("submit", function () {
    convertFile();
}, false);
