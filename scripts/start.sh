export LD_LIBRARY_PATH=../lib
export MINIO_NKV_CONFIG=../conf/nkv_config.json
export MINIO_ACCESS_KEY=minio
export MINIO_SECRET_KEY=minio123
export MINIO_STORAGE_CLASS_STANDARD=EC:2
export MINIO_NKV_MAX_VALUE_SIZE=2097152
export MINIO_NKV_TIMEOUT=20
export MINIO_NKV_SYNC=1
#export MINIO_NKV_CHECKSUM=1
ulimit -n 65535
ulimit -c unlimited
yum install boost-devel
yum install jemalloc-devel

LD_PRELOAD=/lib64/libjemalloc.so.1 ./minio_nkv_jul24 server /dev/nvme{0...5}n1
#LD_PRELOAD=/lib64/libjemalloc.so.1 ./minio_nkv_jul02.1 server /dev/kvemul{1...4}
date

