/**
 *  The Clear BSD License
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *  	* Redistributions of source code must retain the above copyright
 *  	  notice, this list of conditions and the following disclaimer.
 *  	* Redistributions in binary form must reproduce the above copyright
 *  	  notice, this list of conditions and the following disclaimer in
 *  	  the documentation and/or other materials provided with the distribution.
 *  	* Neither the name of Samsung Electronics Co., Ltd. nor the names of its
 *  	  contributors may be used to endorse or promote products derived from
 *  	  this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 *  BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *  BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LIST_LIB_H
#define __LIST_LIB_H

#ifdef __cplusplus
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#include "dragonfly.h"
#include "df_io_module.h"
#include <df_list.h>

#include "utils/dss_hsl.h"

typedef struct list_zone_s {
	int					zone_idx;
	void				*module_instance_ctx;
	std::unordered_map<std::string, std::set<std::string> >* listing_keys;
	dss_hsl_ctx_t *hsl_keys_ctx;
	//char * listing_keys;
} list_zone_t;

typedef struct list_context_s {
	uint32_t	nr_zones;	//total nr of valid maps, multiple maps per pool.
//	dfly_io_module_pool_t **pools;		//pool array.
	dfly_io_module_context_t io_ctx;
	list_zone_t zones[0];		//the whole maps array
} list_context_t;

typedef struct list_thread_inst_ctx {
	list_context_t *mctx;
	int nr_zones;
	list_zone_t *zone_arr[0];
} list_thread_inst_ctx_t;

typedef struct list_prefix_entry_pair {
	const char *prefix;
	const char *entry;
	int prefix_size;
	int entry_size;
} list_prefix_entry_pair_t;

int parse_delimiter_entries_pos(std::string &key, const std::string delimiter,
				std::vector<std::string> &prefixes, std::vector<std::size_t> &positions,
				std::vector<std::string> &entries);
int list_io(void *ctx, struct dfly_request *req, int list_op_flags);

#ifdef __cplusplus
}
#endif

#endif //___LIST_LIB_H

