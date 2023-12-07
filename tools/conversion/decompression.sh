#!/bin/bash


sudo apt update && sudo apt install cmake && sudo apt install build-essential && sudo apt install awscli

cmake .. -DCMAKE_BUILD_TYPE=Release

make decompression-speed

aws s3 sync s3://public-bi-eu-central-1/v0.0.1/btr/ ./data --no-sign

# Directory containing folders
directory="./data"
output_file="decompression-output.txt"
threads=32
reps=100
typefilter=""

if [ "$#" -ge 3 ]; then
	threads=$1
	reps=$2
	typefilter=$3
fi	

echo "running benchmark with $threads threads and $reps reps"

# Check if the directory exists
if [ -d "$directory" ]; then
    # Remove existing output file or create an empty one
    > "$output_file"

    # Iterate through each folder in the directory
    for folder in "$directory"/*/; do
        # Call your script with the folder directory as an argument and append output to the file
        ./decompression-speed -btr "$folder" -threads "$threads" -reps "$reps" -typefilter "$typefilter"  >> "$output_file"
    done
else
    echo "Directory not found!"
fi
