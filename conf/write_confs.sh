#!/bin/bash
# compares and copies (after you permit) config files located in ~/conf
#  to /etc and /boot and whereever needed. Must run as root!
#  written by Daniel Piri (http://opendatalogger.com)
#set -e
IFS=$'\n' read -d '' -r -a lines < files.txt

#echo -e "${lines[@]}"

for i in "${lines[@]}"; do
    LF=$(echo $i | cut -c 2-)
    echo -ne "$LF \t<-> $i \t"
    touch $i
    DIFF=$(git diff $LF $i)
    if [ "$DIFF" == "" ]; then
        echo '[IDENT]'
    else
        #echo -e "\n$DIFF\n"
        git diff $i $LF
        read -p "overwrite $i (y/n)? " choice
        case "$choice" in
          y|Y )
                if [[ $EUID -ne 0 ]]; then
                    echo -e "You must be root!"
                    exit 1
                else
                    cp $i $i.bck
                    echo -e "\$ cp $LF $i :"
                    cp $LF $i
                fi
              ;;
          * ) ;;
        esac
    fi
done


