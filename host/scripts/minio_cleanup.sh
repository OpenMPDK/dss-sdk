#!/usr/bin/env bash
export LD_LIBRARY_PATH=../lib
start_drive=$1
end_drive=$2
conf_file=$3
[ $# -lt 2 ] && { echo "Usage: $0 <start_drive> <end_drive> <nkv_config.json>"; exit 1; }
for i in $(eval echo "{$start_drive..$end_drive}");
do 
    ./nkv_test_cli -c ../conf/"$conf_file" -i msl-ssg-dl04 -p 1030 -w meta/.minio.sys/format.json -s /dev/nvme"${i}"n1 -o 2 -v 2097152
    ./nkv_test_cli -c ../conf/"$conf_file" -i msl-ssg-dl04 -p 1030 -w meta/.minio.sys/kv-volumes -s /dev/nvme"${i}"n1 -o 2 -v 2097152
    ./nkv_test_cli -c ../conf/"$conf_file" -i msl-ssg-dl04 -p 1030 -w meta/.minio.sys/config/config.json/xl.json -s /dev/nvme"${i}"n1 -o 2 -v 2097152
done

