#!/bin/bash
last=""
md5sum captured/*.rsed | sort | \
while read i; do
    v=($i)
    hash=${v[0]}
    file=${v[1]}
    if [[ $hash != $last ]]
    then
        echo $file $hash
        last=$hash
    else 
        base=`basename $file .rsed`
        rm captured/$base.* captured/$base-save*.in
    fi
done
