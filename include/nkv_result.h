#ifndef NKV_RESULT_H
#define NKV_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

// kvs_result messages;	
typedef enum {	
  // generic command status	
  NKV_SUCCESS=0, // success
  //NKV Errors starting from > 1000 err code
  NKV_ERR_UNREACHABLE = 0x001, // Not able to communicate with NKV-FM
  NKV_ERR_CONFIG = 0x002,    // Error parsing nkv config file
  NKV_ERR_NULL_INPUT= 0x003,  // config_file or app_uuid is NULL
  NKV_ERR_INTERNAL = 0x004,    // Error parsing nkv config file
  NKV_ERR_COMMUNICATION = 0x005, // Error communicating with devices/Paths, path open failed ?
  NKV_ERR_ALREADY_INITIALIZED = 0x006,
  NKV_ERR_WRONG_INPUT = 0x007,
  NKV_ERR_INS_STOPPING = 0x008,
  NKV_ERR_NO_CNT_FOUND = 0x009,
  NKV_ERR_HANDLE_INVALID = 0x00A,
  NKV_ERR_INVALID_START_INDEX = 0x00B,
  NKV_ERR_NO_CNT_PATH_FOUND = 0x00C,
  NKV_ERR_KEY_LENGTH = 0x00D,
  NKV_ERR_VALUE_LENGTH = 0x00E,
  NKV_ERR_IO = 0x00F,
  NKV_ERR_OPTION_ENCRYPTION_NOT_SUPPORTED = 0x010,
  NKV_ERR_OPTION_CRC_NOT_SUPPORTED = 0x011,
  NKV_ERR_BUFFER_SMALL = 0x012,
  NKV_ERR_CNT_INITIALIZED = 0x013,
  NKV_ERR_COMMAND_SUBMITTED = 0x014,
  NKV_ERR_CNT_CAPACITY = 0x015,
  NKV_ERR_KEY_EXIST = 0x016,
  NKV_ERR_KEY_INVALID = 0x017,
  NKV_ERR_KEY_NOT_EXIST = 0x018,
  NKV_ERR_CNT_BUSY = 0x019,
  NKV_ERR_CNT_IO_TIMEOUT = 0x01A,
  NKV_ERR_VALUE_LENGTH_MISALIGNED = 0x01B,
  NKV_ERR_VALUE_UPDATE_NOT_ALLOWED = 0x01C,
  NKV_ERR_PERMISSION = 0x01D,
  NKV_ERR_MAXIMUM_VALUE_SIZE_LIMIT_EXCEEDED = 0x01E,
  NKV_ITER_MORE_KEYS = 0x01F
  

} nkv_result;  

#ifdef __cplusplus
} // extern "C"
#endif

#endif
