#!/bin/bash

# Read scenes.txt line by line
while IFS= read -r line
do

    echo "$line is being rendered..."

    if [ ! -f "checkpoints/$line.bvh.txt" ]; then
        start=$(date +%s%N)
        ./raytracer $line configuration.json
        end=$(date +%s%N)
        echo "BVH : $(($(($end-$start))/1000000)) ms"
        echo "$(($(($end-$start))/1000000))" > "checkpoints/$line.bvh.txt"
    fi

    # if [ ! -f "$line.bvh.disabled.txt" ]; then
    #     start=$(date +%s%N)
    #     ./raytracer $line configuration_bvh_disabled.json
    #     end=$(date +%s%N)
    #     echo "BVH : $(($(($end-$start))/1000000)) ms"
    #     echo "$(($(($end-$start))/1000000))" > "checkpoints/$line.bvh.disabled.txt"
    # fi

done < scenes.txt