var appInitialised = false;
var dmf2mod_app = document.getElementById('dmf2mod_app');

Module['print'] = function (e) {
    //std::cout redirects to here
    console.log(e);
}

Module['printErr'] = function (e) {
    //std::cerr redirects to here
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

    // Add module-specific command-line options
    for (let i in availMods) {
        const m = availMods[i];
        var optionsHTML = '<div id="div_' + m + '" class="module_options" style="display:';
        if (i == availMods.length - 1)
            optionsHTML += ' block;">';
        else
            optionsHTML += ' none;">';
        
        var options = Module.getAvailableOptions(m);
        if (!options || options.length == 0) {
            optionsElement.innerHTML += optionsHTML + '<br>(No options)</div>';
            continue;
        }

        optionsHTML += '<br>Options:<br>';
        var optionsArray = options.split(';');
        for (let j in optionsArray) {
            const o = optionsArray[j];
            const found = re.exec(o);
            if (!found || arrayIsEmpty(found)) {
                optionsElement.innerHTML += optionsHTML + 'Error loading options</div>';
                continue;
            }

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
                    optionsHTML += '--' + flagNameLong + ' ' + getSliderHTML(m, flagNameLong, false);
                }
                else if (!isAnything) { // The flag has a few pre-defined options to set it to
                    optionsHTML += '--' + flagNameLong + '=' + getDropdownHTML(m, flagNameLong, values);
                } else { // The flag can be set to any string
                    optionsHTML += '--' + flagNameLong + '=' + getTextboxHTML(m, flagNameLong);
                }
                optionsHTML += '<br>';
            } else if (flagNameShort) {
                if (!hasValues) { // The flag isn't set to anything. Its existence/non-existence represents a boolean.
                    optionsHTML += '-' + flagNameShort + ' ' + getSliderHTML(m, flagNameShort, false);
                }
                else if (!isAnything) { // The flag has a few pre-defined options to set it to
                    optionsHTML += '-' + flagNameShort + '=' + getDropdownHTML(m, flagNameShort, values);
                } else { // The flag can be set to any string
                    optionsHTML += '-' + flagNameShort + '=' + getTextboxHTML(m, flagNameShort);
                }
                optionsHTML += '<br>';
            }
        }
        optionsElement.innerHTML += optionsHTML + '</div>';
    }
}

var getSliderHTML = function(id, flagName, checked) {
    var html = '<input type="checkbox" id="' + id + '_' + flagName + '" class="' + id + '_' + 'option" name="' + flagName + '"';
    if (checked)
        html += ' checked';
    html += '>';
    return html;
}

var getDropdownHTML = function(id, flagName, dropdownOptions) {
    var html = '<select id="' + id + '_' + flagName + '" class="' + id + '_' + 'option" name="' + flagName + '">';
    for (let i in dropdownOptions) {
        const option = dropdownOptions[i];
        html += '<option value="' + option + '">' + option + '</option>';
    }
    html += '</select>';
    return html;
}

var getTextboxHTML = function(id, flagName) {
    return '<input type="text" id="' + id + '_' + flagName + '" class="' + id + '_' + 'option" name="' + flagName + '">';
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

var getOutputType = function() {
    return document.querySelector('input[name="output_type"]:checked').value;
}

var getCommandLineArgs = function() {
    const currentOutputModule = getOutputType();
    const optionsForCurrentModule = document.getElementsByClassName(currentOutputModule  + '_option');
    
    var arguments = '';
    const len = optionsForCurrentModule.length;
    for (let i = 0; i < len; i++) {
        const option = optionsForCurrentModule[i];
        var flagName = '';
        if (option.name.length == 1)
            flagName = '-' + option.name;
        else
            flagName = '--' + option.name;

        if (option.nodeName.toLowerCase() == 'select') { // dropdown
            arguments += flagName + '=' + option.value;
            if (i != len - 1)
                arguments += '\n'; // Using newline delimiter b/c text box value can contain spaces
        } else if (option.nodeName.toLowerCase() == 'input') {
            if (option.type == 'checkbox') { // checkbox
                if (option.checked) {
                    arguments += flagName;
                    if (i != len - 1)
                        arguments += '\n'; // Using newline delimiter b/c text box value can contain spaces
                } 
            } else if (option.type == 'text') { // text box
                arguments += flagName + '="' + option.value + '"';
                if (i != len - 1)
                    arguments += '\n'; // Using newline delimiter b/c text box value can contain spaces
            }
        }
    }
    return arguments;
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

var convertFile = async function() {
    // Check if input file was provided
    const inputFileElem = document.getElementById('inputFile');
    if (inputFileElem.value.length == 0)
        return true;

    // Get input file and input filename to be used internally
    const externalFile = inputFileElem.files.item(0);
    const internalFilenameInput = externalFile.name;

    // Change file extension to get internal filename for output from dmf2mod
    const outputType = getOutputType();
    var internalFilenameOutput = internalFilenameInput;
    internalFilenameOutput = internalFilenameOutput.replace(/\.[^/.]+$/, '') + '.' + outputType;
    
    if (internalFilenameInput == internalFilenameOutput) {
        // No conversion needed: Converting to same type
        return true;
    }

    var resp = await importFileLocal(externalFile, internalFilenameInput);
    if (resp === true)
        return true;

    resp = Module.moduleImport(internalFilenameInput);
    if (resp === true)
        return true;
    
    let stream = FS.open(internalFilenameOutput, 'w+');
    FS.close(stream);

    const commandLineArgs = getCommandLineArgs();
    const result = Module.moduleConvert(internalFilenameOutput, commandLineArgs);
    if (result != internalFilenameOutput)
        return true;
    
    const byteArray = FS.readFile(internalFilenameOutput);
    const blob = new Blob([byteArray]);
    
    var a = document.createElement('a');
    a.download = internalFilenameOutput;
    a.href = window.URL.createObjectURL(blob);
    a.click();
}
