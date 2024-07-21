#!/usr/bin/env bash

GENERATOR=

THIS_DIR=$(dirname "${0}")
(
  cd "${THIS_DIR}" &&
  mkdir -p build &&
  cd build && 
  cmake "${@}" ../src &&
  cmake --build . --parallel
)
