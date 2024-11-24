#!/bin/bash

echo "" > benchmark.txt
rm *.png

# Read scenes.txt line by line
while IFS= read -r line
do

    echo "##############" >> benchmark.txt
    echo "$line is being rendered..." >> benchmark.txt
    ./raytracer $line configuration.json >> benchmark.txt
    echo "##############" >> benchmark.txt

done < scenes.txt