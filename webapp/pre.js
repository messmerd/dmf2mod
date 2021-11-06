// Set up IDBFS file system
// From: https://badlydrawnrod.github.io/posts/2020/06/07/emscripten-indexeddb/
Module['preRun'] =  function (e) {
    FS.mkdir('/working');
    FS.mount(IDBFS, {}, '/working');
}
