// Set up IDBFS file system
// From: https://badlydrawnrod.github.io/posts/2020/06/07/emscripten-indexeddb/
Module['preRun'] =  function (e) {
    FS.mkdir('/working');
    FS.mount(IDBFS, {}, '/working');
}

Module['print'] = function (e) {
    //std::cout redirects to here
    console.log(e);
}

Module['printErr'] = function (e) {
    //std::cerr redirects to here
    console.log(e);
    statusMessage += e + '\n';
}
