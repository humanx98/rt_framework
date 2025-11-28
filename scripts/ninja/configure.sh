#!/bin/bash

set -e

script_dir=$(dirname $0)
root_dir=$(realpath "$script_dir/../..")

cmake -S "$root_dir" -B "$root_dir/build/ninja" -G Ninja -DCMAKE_BUILD_TYPE=Debug
