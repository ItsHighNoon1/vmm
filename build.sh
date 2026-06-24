#!/bin/bash

# ItsHighNoon's C build script
#
# Last modified 06/24/2026

readarray -t flags < compile_flags.txt

source_files=()
while IFS= read -r line; do
    source_files+=("${line#src/}")
done < <(find "src" -type f -name "*.c")

rm -rf obj
mkdir -p obj
object_files=()
for source in "${source_files[@]}"; do
    object="obj/${source%.*}.o"
    object_files+=("$object")
    echo "Building $source"
    dir="${object%/*}"
    mkdir -p $dir
    emcc -c -o "$object" "src/$source" $(IFS=$'\n'; echo "${flags[*]}") &
done
wait

emcc $(IFS=$'\n'; echo "${flags[*]}") -o vmm.js --js-library src/library.js --use-port=sdl3 -sASYNCIFY $(IFS=$'\n'; echo "${object_files[*]}")