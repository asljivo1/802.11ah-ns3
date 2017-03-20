#!/bin/bash

if [ -z $1 ]; then
        inputdir=$PWD
else
        inputdir=$1
fi

mkdir -p $inputdir/trimmed
for filename in $inputdir/*.nss; do


        outputfile="$inputdir/trimmed/$(basename $filename)"

        if [ ! -f $outputfile ]; then
                if [[ $(find $filename -type f -size +10485760c 2>/dev/null) ]]; then
                        echo "Writing to " $outputfile
                        grep -E "(^0;|\d?99000000000)" $filename > $outputfile
                else
                        echo "Skipping $filename, it is smaller than 10mb"
                fi
        else
                echo "Skipping $outputfile, already exists"
        fi

done






