#!/bin/bash

set -e

script_dir=$(dirname $0)
root_dir=$(realpath "$script_dir/../..")

cmake -S "$root_dir" -B "$root_dir/build/vs" -G "Visual Studio 17 2022" -A x64
