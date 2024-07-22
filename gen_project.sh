#!/usr/bin/env bash

GENERATOR=

THIS_DIR=$(dirname "${0}")

cd "${THIS_DIR}" &&
mkdir -p build

if ! (
  cd build && 
  cmake "${@}" ../src &&
  cmake --build . --parallel
); then
  exit 1
fi

