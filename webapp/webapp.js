var appInitialised = false;

Module['print'] = function (e) {
    //std::cout redirects to here
    console.log(e);
};

Module['onRuntimeInitialized'] = function () {
    console.log("Loaded dmf2mod");
    appInitialised = true;
    document.getElementById('status').innerText = "Status: Loaded!";
    
};

var isWebAssemblySupported = function() {
    return typeof WebAssembly === 'object' ? true : false;
}

// From: https://stackoverflow.com/questions/63959571/how-do-i-pass-a-file-blob-from-javascript-to-emscripten-webassembly-c 
// Imports file into file system so that it can be accessed by WASM program.
// extrnalFile is the file to import and it can be a local file or URL. internalFilename is the name to use.
// Returns true if it failed.
var importFile = async function(externalFile, internalFilename) {
    if (!appInitialised)
        return true;
    
    // Download a file
    let blob;
    try {
        blob = await fetch(externalFile).then(response => {
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
    var resp = await importFile(externalFile, internalFilenameInput);
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


var onClickTest = function(e) {
    console.log("In onClickTest");
    e.preventDefault();

    if (appInitialised === true) {
        var output = Module.getAvailableModules();
        document.getElementById("output").innerHTML = output;
    } else {
        console.log("Waiting for dmf2mod to be initialised");
    }

    return false;
}

/*

script.onload = function () {
    //Module['preRun'] =  function (e) {
    //
    //};
    
    Module['print'] = function (e) {
        //std::cout redirects to here
        console.log(e);
    };
    Module['onRuntimeInitialized'] = function () {
        console.log("Loaded dmf2mod!");
        appInitialised = true;
        document.getElementById('status').innerText = "Status: Loaded!";
        
    };

    
    .then(function(m) {
        // For some reason, I cannot access any of the functions unless I use the object passed here
        app = m;
    });
    
    Module.getAvailableModules();
};

document.body.appendChild(script);
*/