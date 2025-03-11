/*
 * pre.js
 * Written by Dalton Messmer <messmer.dalton@gmail.com>.
 *
 * Input to emscripten compiler.
 */

/*global Module, FS, errorMessage, warningMessage, statusMessageIsError*/

// Set up IDBFS file system
// From: https://badlydrawnrod.github.io/posts/2020/06/07/emscripten-indexeddb/
Module["preRun"] = function () {
  FS.mkdir("/working");
  FS.mount(IDBFS, {}, "/working");
}

Module["print"] = function (e) {
  //std::cout redirects to here
  console.log(e);
}

Module["printErr"] = function (e) {
  //std::cerr redirects to here
  //console.log(e);
  if (statusMessageIsError)
    errorMessage += e + "\n";
  else
    warningMessage += e + "\n";
}
