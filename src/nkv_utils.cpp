/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include "nkv_utils.h"

int32_t nkv_cmd_exec(const char* cmd, std::string& result) {
  std::array<char, 512> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    return -1;
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
  }
  return 0;
}

nkv_result nkv_get_path_stat_util (const std::string& p_mount, nkv_path_stat* p_stat) {
  std::string cmd_str = NKV_DEFAULT_STAT_FILE;
  if(const char* env_p = std::getenv("NKV_STAT_SCRIPT")) {
    cmd_str = env_p;
  }
  smg_info(logger, "NKV stat file = %s", cmd_str.c_str());

  cmd_str += " ";
  cmd_str += p_mount;
  smg_info(logger, "NKV stat command = %s", cmd_str.c_str());
  std::string result;
  int32_t rc = nkv_cmd_exec(cmd_str.c_str(), result);
  if (rc == 0) {
    smg_info(logger, "NKV stat data = %s", result.c_str());
    boost::property_tree::ptree pt;
    try {
      std::istringstream is (result);
      boost::property_tree::read_json(is, pt);
    }
    catch (std::exception& e) {
      smg_error(logger, "Error reading NKV stat output, cmd = %s, buffer = %s ! Error = %s",
               cmd_str.c_str(), result.c_str(), e.what());
      return NKV_ERR_INTERNAL;
    }
    p_mount.copy(p_stat->path_mount_point, p_mount.length());
    try {
      BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt) {
        assert(v.first.empty());
        boost::property_tree::ptree pr = v.second;
        std::string d_cap_str = p_mount + ".DiskCapacityInBytes";
        std::string d_usage_str = p_mount + ".DiskUtilizationInBytes";
        std::string d_usage_percent_str = p_mount + ".DiskUtilizationPercentage";
        p_stat->path_storage_capacity_in_bytes = pr.get<uint64_t>(d_cap_str, 0);
        p_stat->path_storage_usage_in_bytes = pr.get<uint64_t>(d_usage_str, 0);
        p_stat->path_storage_util_percentage = pr.get<double>(d_usage_percent_str, 0);
      }
    }
    catch (std::exception& e) {
      smg_error(logger, "Error reading NKV stat output, cmd = %s, buffer = %s ! Error = %s",
               cmd_str.c_str(), result.c_str(), e.what());
      return NKV_ERR_INTERNAL;
    }

  } else {
    smg_error(logger, "NKV stat script execution failed, cmd = %s !!", cmd_str.c_str());
    return NKV_ERR_INTERNAL;
  }
  return NKV_SUCCESS;
}

