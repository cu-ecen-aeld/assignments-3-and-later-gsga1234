#!/bin/bash

# Check if both arguments are provided
if [ $# -ne 2 ]
then
    echo "Error: Two arguments required."
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if filesdir exists and is a directory
if [ ! -d "$filesdir" ]
then
    echo "Error: $filesdir is not a valid directory."
    exit 1
fi

# Count number of files recursively
file_count=$(find "$filesdir" -type f | wc -l)

# Count number of matching lines
match_count=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $file_count and the number of matching lines are $match_count"

