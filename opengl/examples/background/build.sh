#!/bin/sh
emcc -std=c++11 --shell-file ../templates/shell.html  -O2 -I ../libs/emscripten -I ../libs  -s NO_EXIT_RUNTIME=1 -s WASM=1 -s USE_WEBGL2=1 -s USE_GLFW=3 --js-library ../libs/emscripten/library_glfw.js --js-library ../libs/emscripten/library_gl.js background.cpp -o out/background.html

