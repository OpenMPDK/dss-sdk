#ifndef NKV_CONST_H
#define NKV_CONST_H

#ifdef __cplusplus
extern "C" {
#endif

#define NKV_CONFIG_FILE "/etc/nkv/nkv_config.json"

#define NKV_TRANSPORT_NVMF_TCP_KERNEL 0
#define NKV_TRANSPORT_NVMF_TCP_SPDK 1
#define NKV_TRANSPORT_NVMF_RDMA_KERNEL 2
#define NKV_TRANSPORT_NVMF_RDMA_SPDK 3 
#define NKV_MAX_ENTRIES_PER_CALL 16 
#define NKV_MAX_CONT_NAME_LENGTH 256 
#define NKV_MAX_CONT_TRANSPORT 32 //For NKV it will be max 4 but big number is to support LKV 
#define NKV_MAX_IP_LENGTH 32 
#define NKV_MAX_KEY_LENGTH 256 
#define NKV_MAX_VALUE_LENGTH 2097152 //2MB value support 

#ifdef __cplusplus
} // extern "C"
#endif

#endif

