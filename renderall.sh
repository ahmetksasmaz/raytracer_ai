#!/bin/bash

# Read scenes.txt line by line
while IFS= read -r line
do

    echo "$line is being rendered..."

    start=$(gdate +%s%N)
    ./raytracer $line configuration.json
    end=$(gdate +%s%N)
    echo "BVH : $(($(($end-$start))/1000000)) ms"

done < scenes.txt