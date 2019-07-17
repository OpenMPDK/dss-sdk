#ifndef NKV_UTILS_H
#define NKV_UTILS_H

#include <iostream>
#include <memory>
#include <string>
#include <array>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include "nkv_struct.h"
#include "nkv_result.h"
#include "csmglogger.h"

extern c_smglogger* logger;
#define NKV_DEFAULT_STAT_FILE "./disk_stats.py"

int32_t nkv_cmd_exec(const char* cmd, std::string& result); 

nkv_result nkv_get_path_stat_util (const std::string& p_mount, nkv_path_stat* p_stat);

#endif
