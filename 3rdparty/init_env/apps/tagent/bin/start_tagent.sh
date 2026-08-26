#!/bin/sh
if [ $UID != 30000 ];then
    echo "Error:Please use the account [user00]"
    exit -1 
fi

./tagent --tlogconf=../cfg/tagent_log.xml --conf-file=../cfg/tagent.xml --log-file="../log/tagent" start -D
