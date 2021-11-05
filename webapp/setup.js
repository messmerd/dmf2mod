
// From: https://badlydrawnrod.github.io/posts/2020/06/07/emscripten-indexeddb/
Module['preRun'] =  function (e) {
    FS.mkdir('/assets');
    FS.mount(IDBFS, {}, '/assets');
};

