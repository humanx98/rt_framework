#!/bin/bash

set -e

script_dir=$(dirname $0)
root_dir=$(realpath "$script_dir/../..")

"$script_dir/build.sh"

cd "$root_dir/build/ninja/src/rt_framework"
./rt_framework
