#!/bin/bash

set -e

script_dir=$(dirname $0)
root_dir=$(realpath "$script_dir/..")

cd $root_dir/submodules/HIPRT
git clean -fdx
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBITCODE=OFF -DGENERATE_BAKE_KERNEL=OFF -DBAKE_COMPILED_KERNEL=ON -DPRECOMPILE=ON -DNO_UNITTEST=ON -DFORCE_DISABLE_CUDA=ON
cmake --build build --config Release
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBITCODE=OFF -DGENERATE_BAKE_KERNEL=OFF -DBAKE_COMPILED_KERNEL=ON -DPRECOMPILE=ON -DNO_UNITTEST=ON -DFORCE_DISABLE_CUDA=ON
cmake --build build/debug --config Debug

cd scripts/bitcodes
python compile.py
