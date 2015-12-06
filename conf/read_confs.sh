#!/bin/bash
IFS=$'\n' read -d '' -r -a lines < files.txt

for i in "${lines[@]}"; do
	LF=$(echo $i | cut -c 2-)
	echo -ne "$i -> $LF \t"
	RES=$(cp --parents $i .)
	if [ "$RES" == "" ]; then
		echo '[copied]'
	else
		echo -e "\nERROR while copy!\n$RES\n\n"
	fi
done







