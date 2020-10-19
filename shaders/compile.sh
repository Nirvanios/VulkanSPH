#!/bin/bash

rm *.spv

for file in ./*; do
    if [ "$file" != "./compile.sh" ]; then
      echo "Compiling $file ..."
      glslc "$file" -o "$file.spv";
    fi
done