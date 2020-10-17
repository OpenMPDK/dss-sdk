export LD_LIBRARY_PATH=../lib
# Configuration file for Local KV
export MINIO_NKV_CONFIG=../conf/nkv_config.json
# Configuration file for Remote KV
export MINIO_ACCESS_KEY=minio
export MINIO_SECRET_KEY=minio123
export MINIO_STORAGE_CLASS_STANDARD=EC:2
export MINIO_NKV_MAX_VALUE_SIZE=1048576
export MINIO_NKV_TIMEOUT=20
export MINIO_NKV_SYNC=1
export MINIO_NKV_SHARED_SYNC_INTERVAL=2
#export MINIO_ENABLE_STATS=1
#export MINIO_NKV_CHECKSUM=1
# Following two switches are required for distributed Minio.
if [[ $1 == "remote" ]]; then
  export MINIO_NKV_CONFIG=../conf/nkv_config_remote.json
  export MINIO_NKV_SHARED_SYNC_INTERVAL=2
  export MINIO_NKV_SHARED=1
fi
if [[ $1 == "remote" && $2 == "dist" ]]; then
  export MINIO_PER_HOST_INSTANCE_COUNT=1
fi  

ulimit -n 65535
ulimit -c unlimited
yum install boost-devel
yum install libcurl-devel


# Copy appropriate Minio binary to current directory.
if [[ $1 == "remote" && $2 == "dist" ]]; then
./minio server http://minio{1...4}/dev/nvme{0...3}n1
else
./minio server /dev/nvme{0...3}n1
fi
#LD_PRELOAD=/lib64/libjemalloc.so.1 ./minio_nkv_jul02.1 server /dev/kvemul{1...4}
date

