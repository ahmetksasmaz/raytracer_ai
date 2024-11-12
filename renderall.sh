#!/bin/bash

# Read scenes.txt line by line
while IFS= read -r line
do

    echo "$line is being rendered..."

    start=$(date +%s%N)
    ./raytracer $line configuration.json
    end=$(date +%s%N)
    echo "BVH : $(($(($end-$start))/1000000)) ms"

done < scenes_debug.txt