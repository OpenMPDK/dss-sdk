#!/usr/bin/env bash
regex="([0-9]+)[ ]+([0-9]+)[ ]+(python){0,1}.+?kv-cli.+?daemon.+?$"
pid=-1
OUTPUT=$(ps xao pid,ppid,args)
IFS=$'\n' read -rd '' -a y <<<"$OUTPUT"
proc_idx=0

for line in "${y[@]}"
do
    if [[ $line =~ $regex ]]
    then
        PROC_PPID_TO_PID[${BASH_REMATCH[2]}]=${BASH_REMATCH[1]}
        ((PROC_PID_CNT[\${BASH_REMATCH[1]}]++))
        ((PROC_PID_CNT[\${BASH_REMATCH[2]}]++))
        ((proc_idx++))
        pid=${BASH_REMATCH[1]}
    fi
done

if (( "$proc_idx" != 1 ))
then
    pid=-1
    for i in "${!PROC_PID_CNT[@]}"
    do
        if (( "${PROC_PID_CNT[$i]}" > 1 ))
        then
            pid=${PROC_PPID_TO_PID[$i]}
        fi
    done
fi

if (( "$pid" > 1 ))
then
    echo "SIGHUP to $pid"
    kill -n 1 "$pid"
fi
