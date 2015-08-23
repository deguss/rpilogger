#!/bin/bash
IFS=$'\n' read -d '' -r -a lines < files.txt

#echo -e "${lines[@]}"

for i in "${lines[@]}"; do
	LF=$(echo $i | cut -c 2-)
	echo -ne "$LF \t<-> $i \t"
	touch $i
	DIFF=$(diff $LF $i)
	if [ "$DIFF" == "" ]; then
		echo '[IDENT]'
	else
		echo -e "\n$DIFF\n"
		read -p "overwrite $i (y/n)? " choice
		case "$choice" in 
		  y|Y ) 
				if [[ $EUID -ne 0 ]]; then
  					echo -e "You must be root!"
					exit 1
				else
					echo -e "\$ cp $LF $i :"
		        	cp $LF $i
				fi
		      ;;
		  * ) ;;
		esac
	fi
done


#
#echo -e "${lines[1]}"
#echo -e "${lines[2]}"


