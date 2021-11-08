#!/usr/bin/env bash
FORMAT="%aN,<%aE>"                   #Set git output format in "Name,<Email>" //Capital in aN and aE means replace str based on .mailmap
TARGET=(examples lkmpg.tex)          #Target files we want to trace
DIR=`git rev-parse --show-toplevel`  #Get root dir of the repo
TARGET=("${TARGET[@]/#/$DIR/}")      #Concat $DIR BEFORE ALL elements in array TARGET

#The str in each line should be Username,<useremail>
function gen-raw-list()
{
    git log --pretty="$FORMAT" ${TARGET[@]} | sort -u
}

function parse-list()
{
    > Contributors  # Clear contributors' list (Overwrite with null)
    while read -r line; do
        User=`echo "$line" | awk -F "," '{print $1}'`   
        if [[ `grep -w "$User" Exclude` ]]; then
            echo "[skip] $User"
            continue;
        fi
        echo "[Add] $User"
        MainMail=`echo "$line" | awk -F "[<*>]" '{print $2}'`
        Emails=(`cat $DIR/.mailmap | grep -w "$User" | awk -F "[<*>]" '{print $4}' | sort -u`)
        for Email in ${Emails[@]}; do
            if [[ "$Email" != "$MainMail" ]]; then
                line="$line,<$Email>";
            fi
        done
        echo "$line" >> Contributors
    done <<< $(gen-raw-list)
    cat Include >> Contributors
}

function sort-list()
{
    if [[ `git diff Contributors` ]]; then
        sort -f -o Contributors{,}
        git add $DIR/scripts/Contributors
    fi
}

#For all lines before endline, print "name, % <email>"
#For endline print "name. % <email>"
function gen-tex-file()
{
    cat Contributors | awk -F "," \
    '   BEGIN{k=0}{name[k]=$1;email[k++]=$2}
        END{
           for(i=0;i<k;i++){
               name[i]=(i<k-1)?name[i]",":name[i]".";
               printf("%-30s %% %s\n",name[i],email[i]);
               }
           }
    '
}

parse-list
sort-list
gen-tex-file > $DIR/contrib.tex