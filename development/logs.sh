#!/bin/bash
set -e

STR="tail "
FILES=$(cat logfiles.txt)

for f in $FILES
do
 STR="$STR$(echo -e "-f $f ")"
done

echo "$STR"
