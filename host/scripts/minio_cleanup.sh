export LD_LIBRARY_PATH=../lib
for i in {0..7}; do ./nkv_test_cli -c ../conf/nkv_config.json -i msl-ssg-dl04 -p 1030 -w meta/.minio.sys/format.json -s /dev/nvme${i}n1 -o 2 -v 2097152; done
for i in {0..7}; do ./nkv_test_cli -c ../conf/nkv_config.json -i msl-ssg-dl04 -p 1030 -w meta/.minio.sys/kv-volumes -s /dev/nvme${i}n1 -o 2 -v 2097152; done
for i in {0..7}; do ./nkv_test_cli -c ../conf/nkv_config.json -i msl-ssg-dl04 -p 1030 -w meta/.minio.sys/config/config.json/xl.json -s /dev/nvme${i}n1 -o 2 -v 2097152; done

