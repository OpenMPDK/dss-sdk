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


#include <stdlib.h>
#include "ustat.h"

#define MAX_KD_DEVICES (64)
#define MAX_KD_DEV_STR (256)

typedef enum kd_type_e {
	DFLY_KD_SIMPLE_HASH = 0,
	DFLY_KD_RH_MURMUR3,
	DFLY_KD_CH_MURMUR3,
} kd_type_t;

struct dfly_kd_fn_table {
	/** Add new devices to the instance **/
	bool (*add_device)(void *kd_ctx, const char *dev_name, uint32_t len, void *device);
	/** Remove Devices from instance **/
	/** Find device for a  Key **/
	void *(*find_device)(void *kd_ctx, void *key, uint32_t len);
	void *(*list_device)(void *kd_ctx, void **dev_list, uint32_t *nr_dev);

};

struct dfly_kd_context_s {
	struct dfly_kd_fn_table *kd_fn_table;
};


int dfly_init_kd_context(uint32_t ssid, kd_type_t type);
void dfly_deinit_kd_context(uint32_t ssid, kd_type_t type);

struct dfly_kd_context_s *dfly_init_kd_rh_context(void);
void dfly_deinit_kd_rh_context(void *vctx);

struct dfly_kd_context_s *dfly_init_kd_sh_context(void);

bool dfly_kd_add_device(uint32_t ssid, const char *device_name, uint32_t len, void *disk);
void *dfly_kd_get_device(struct dfly_request *req);
void *dfly_kd_key_to_device(uint32_t ssid, void *key, uint32_t keylen);
void *dfly_list_device(uint32_t ssid, void **dev_list, uint32_t *nr_dev);

