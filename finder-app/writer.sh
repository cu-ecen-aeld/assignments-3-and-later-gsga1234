#!/bin/bash

# Check arguments
if [ $# -ne 2 ]
then
    echo "Error: Two arguments required."
    exit 1
fi

writefile=$1
writestr=$2

# Extract directory path
writedir=$(dirname "$writefile")

# Create directory if it doesn't exist
mkdir -p "$writedir"

# Write string to file
echo "$writestr" > "$writefile"

# Check if file creation succeeded
if [ $? -ne 0 ]
then
    echo "Error: Could not create file $writefile"
    exit 1
fi

