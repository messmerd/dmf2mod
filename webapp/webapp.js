var appInitialised = false;
var dmf2mod_app = document.getElementById('dmf2mod_app');

var onSubmit = function() {
    var inputFile = document.getElementById('inputFile').files.item(0);
    return processFile(inputFile, 'temp.dmf', 'temp.mod', 'mod');
}

Module['print'] = function (e) {
    //std::cout redirects to here
    console.log(e);
}

Module['onRuntimeInitialized'] = function () {
    console.log("Loaded dmf2mod");
}

Module['postRun'] =  function () {
    console.log("Initialized dmf2mod");
    loadOptions();
    dmf2mod_app.style.display = 'block';
    document.getElementById('loading').style.display = 'none';
    appInitialised = true;
}

var isWebAssemblySupported = function() {
    return typeof WebAssembly === 'object' ? true : false;
}

var arrayIsEmpty = function(a) {
    if (!Array.isArray(a))
        return true;
    if (!a.length)
        return true;
    if (a.length == 1 && !a[0] && a[0].length == 0)
        return true;
    return false;
}

var loadOptions = function() {
    var optionsElement = document.getElementById('options');
    optionsElement.innerHTML = '<label>Output type:</label>';

    var availMods = Module.getAvailableModules().split(',');
    if (arrayIsEmpty(availMods)) {
        optionsElement.innerHTML += '<br>ERROR: No supported modules';
        return;
    }

    // Available options pattern
    const pattern = String.raw`(?:-(\w+)(=\[((?:\w|,)+)?\]+)?,\s+)?(?:--(\w+)(=\[((?:\w|,)+)?\]+)?)?`;
    const re = RegExp(pattern, '');

    // Add available modules to convert to
    for (let i in availMods) {
        const m = availMods[i];
        // Add radio button
        var typesHTML = '<input type="radio" id="' + m + '" name="output_type" value="' + m + '"';
        if (i == availMods.length - 1)
            typesHTML += ' checked'; // Last radio button is checked
        typesHTML += ' onchange="onOutputTypeChange(this)"><label for="' + m + '">' + m + '</label>';
        optionsElement.innerHTML += typesHTML;
    }

    optionsElement.innerHTML += '<br><br>Options:<br>';

    // Add module-specific command-line options
    for (let i in availMods) {
        const m = availMods[i];
        var optionsHTML = '<div id="div_' + m + '" class="module_options" style="display:';
        if (i == availMods.length - 1)
            optionsHTML += ' block;">';
        else
            optionsHTML += ' none;">';
        var options = Module.getAvailableOptions(m);
        var optionsArray = options.split(';');
        for (let j in optionsArray) {
            const o = optionsArray[j];
            const found = re.exec(o);
            if (!found || arrayIsEmpty(found))
                continue;
            const flagNameShort = found[1];
            const flagShortValues = found[3];
            const flagNameLong = found[4];
            const flagLongValues = found[6];

            // Parse acceptable values for option:
            var values = [];
            var hasValues = false;
            if (found[5]) {
                if (flagLongValues)
                    values = flagLongValues.split(',');
                else
                    values = [];
                hasValues = true;
            } else if (found[2]) {
                if (flagShortValues)
                    values = flagShortValues.split(',');
                else
                    values = [];
                hasValues = true;
            }
            
            // Check if any string is acceptable for option:
            var isAnything = false;
            if (hasValues && arrayIsEmpty(values)) {
                isAnything = true;
            }

            if (flagNameLong) {
                if (!hasValues) { // The flag isn't set to anything. Its existence/non-existence represents a boolean.
                    optionsHTML += '--' + flagNameLong + ' ' + getSliderHTML(m + '_' + flagNameLong, false);
                }
                else if (!isAnything) { // The flag has a few pre-defined options to set it to
                    optionsHTML += '--' + flagNameLong + '=' + getDropdownHTML(m + '_' + flagNameLong, values);
                } else { // The flag can be set to any string
                    optionsHTML += '--' + flagNameLong + '=' + getTextboxHTML(m + '_' + flagNameLong);
                }
                optionsHTML += '<br>';
            } else if (flagNameShort) {
                if (!hasValues) { // The flag isn't set to anything. Its existence/non-existence represents a boolean.
                    optionsHTML += '-' + flagNameShort + ' ' + getSliderHTML(m + '_' + flagNameShort, false);
                }
                else if (!isAnything) { // The flag has a few pre-defined options to set it to
                    optionsHTML += '-' + flagNameShort + '=' + getDropdownHTML(m + '_' + flagNameShort, values);
                } else { // The flag can be set to any string
                    optionsHTML += '-' + flagNameShort + '=' + getTextboxHTML(m + '_' + flagNameShort);
                }
                optionsHTML += '<br>';
            }
        }
        optionsElement.innerHTML += optionsHTML;
    }
}

var getSliderHTML = function(id, checked) {
    var html = '<label class="switch" id="' + id + '"><input type="checkbox"';
    if (checked)
        html += ' checked';
    html += '><span class="slider round"></span></label>';
    return html;
}

var getDropdownHTML = function(id, dropdownOptions) {
    var html = '<select id="' + id + '">';
    for (let i in dropdownOptions) {
        const option = dropdownOptions[i];
        html += '<option value="' + option + '">' + option + '</option>';
    }
    html += '</select>';
    return html;
}

var getTextboxHTML = function(id) {
    return '<input type="text" id="' + id + '">';
}

var onOutputTypeChange = function(elem) {
    // Hide command-line options for all modules
    var elements = document.getElementsByClassName('module_options');
    for (const element of elements) {
        element.style.display = 'none';
    }

    // Show command-line options for the current module
    const optionsId = 'div_' + elem.value;
    document.getElementById(optionsId).style.display = 'block';
}

var importFileLocal = async function(externalFile, internalFilename) {
    if (!appInitialised)
        return true;

    // Convert blob to Uint8Array (more abstract: ArrayBufferView)
    let data = new Uint8Array(await externalFile.arrayBuffer());
    
    // Store the file
    let stream = FS.open(internalFilename, 'w+');
    FS.write(stream, data, 0, data.length, 0);
    FS.close(stream);

    return false;
}

// From: https://stackoverflow.com/questions/63959571/how-do-i-pass-a-file-blob-from-javascript-to-emscripten-webassembly-c 
// Imports file into WASM file system so that it can be accessed by WASM program.
// url is the url of the file to import. This can also be a local file. internalFilename is the name to use.
// Returns true if it failed.
var importFileOnline = async function(url, internalFilename) {
    if (!appInitialised)
        return true;
    
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
        console.log('getFileFromURL: ' + error);
        return true;
    }

    if (!blob) {
        console.log('getFileFromURL: Bad response.');
        return true;
    }

    // Convert blob to Uint8Array (more abstract: ArrayBufferView)
    let data = new Uint8Array(await blob.arrayBuffer());
    
    // Store the file
    let stream = FS.open(internalFilename, 'w+');
    FS.write(stream, data, 0, data.length, 0);
    FS.close(stream);

    return false;
}

var convertFile = function(internalFilenameOutput, outputType) {
    let stream = FS.open(internalFilenameOutput, 'w+');
    FS.close(stream);

    return Module.moduleConvert(outputType);
}

var processFile = async function(externalFile, internalFilenameInput, internalFilenameOutput, outputType) {
    var resp = await importFileLocal(externalFile, internalFilenameInput);
    if (resp === true)
        return true;

    resp = Module.moduleImport(internalFilenameInput);
    if (resp === true)
        return true;
    
    resp = convertFile(internalFilenameOutput, outputType);
    if (resp === true)
        return true;
    return false;
}
