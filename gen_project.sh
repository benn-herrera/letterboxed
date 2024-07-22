#!/usr/bin/env bash

GENERATOR=

THIS_DIR=$(dirname "${0}")

cd "${THIS_DIR}" &&
mkdir -p build

if ! (
  cd build && 
  cmake "${@}" ../src &&
  cmake --build . --parallel &&
  cmake --build . --target run_core_suite --target run_word_db_suite
); then
  exit 1
fi
