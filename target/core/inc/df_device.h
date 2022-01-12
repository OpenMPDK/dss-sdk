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


#ifndef _DFLY_DEVICE_H_
#define _DFLY_DEVICE_H_

#define DFLY_MAX_HANDLES (64)

typedef enum devtype_s {
	DFLY_DEVICE_TYPE_BDEV = 0,
	DFLY_DEVICE_TYPE_KV_POOL,
} devtype_t;

typedef void (*df_dev_io_completion_cb)(struct df_dev_response_s resp, void *arg);

struct df_device_ops {
	struct df_device *(*open)(const char *path, bool write);
	void (*close)(struct df_device *device);
	int (*io_complete)(struct dfly_request *req);
	uint64_t (*getsize)(struct df_device *device);
	int (*admin_passthru)(struct df_device *device, struct spdk_nvme_cmd *cmd,
			      void *buff, uint64_t nbytes,
			      df_dev_io_completion_cb cb, void *cb_arg);
	union {
		struct {
			int (*read)(struct df_device *device, void *buff, uint64_t offset,
				    uint64_t nbytes,
				    df_dev_io_completion_cb cb, void *cb_arg);
			int (*write)(struct df_device *device, void *buff, uint64_t offset,
				     uint64_t nbytes,
				     df_dev_io_completion_cb cb, void *cb_arg);
		} blk;
		struct {
			void *(*iter_info_setup)(struct df_device *device, struct dfly_request *req);
			int (*iter_open)(struct df_device *device,
					 void *iter_info,
					 df_dev_io_completion_cb cb, void *cb_arg);
			int (*iter_close)(struct df_device *device,
					  void *iter_info,
					  df_dev_io_completion_cb cb, void *cb_arg);
			int (*iter_read)(struct df_device *device,
					 void *iter_info,
					 df_dev_io_completion_cb cb, void *cb_arg);
			int (*store)(struct df_device *device,
				     struct dfly_key *key, struct dfly_value *val,
				     df_dev_io_completion_cb cb, void *cb_arg);
			int (*retrieve)(struct df_device *device,
					struct dfly_key *key, struct dfly_value *val,
					df_dev_io_completion_cb cb, void *cb_arg);
			int (*key_delete)(struct df_device *device, struct dfly_key *key,
					  df_dev_io_completion_cb cb, void *cb_arg);
			int (*exists)(struct df_device *device, struct dfly_key **key, int nitems);
			int (*build_request)(struct df_device *device, int opc,
					     struct dfly_key *key, struct dfly_value *val,
					     df_dev_io_completion_cb cb, void *cb_arg,
					     struct dfly_request **pp_req);
		} kv;
	} d;
	void (*set_handle)(void *ctx, int handle);/*Used by open*/
};

struct df_device {
	devtype_t type;
	struct df_device_ops ops;
};

struct df_device_module {
	struct df_device *io_device;
	TAILQ_ENTRY(df_device_module) link;
};

struct df_device_mgr {
	struct df_device *ctx[DFLY_MAX_HANDLES];
	pthread_mutex_t ctx_index_lock;

	TAILQ_HEAD(, df_device_module) dfly_dev_modules;
};

bool dfly_bdev_write_cached(const char *path);

int dfly_dev_init(void);
int dfly_device_open(char *path, devtype_t type, bool write);
void dfly_device_close(int handle);
void dfly_device_release_handle(int handle);

uint64_t dfly_device_getsize(int handle);
int dfly_device_write(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		      df_dev_io_completion_cb cb, void *cb_arg);
int dfly_device_read(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		     df_dev_io_completion_cb cb, void *cb_arg);
void *dfly_device_create_iter_info(int handle, struct dfly_request *req);
int dfly_device_iter_open(int handle, void *iter_info,
			  df_dev_io_completion_cb cb, void *cb_arg);
int dfly_device_iter_close(int handle, void *iter_info,
			   df_dev_io_completion_cb cb, void *cb_arg);
int dfly_device_iter_read(int handle, void *iter_info,
			  df_dev_io_completion_cb cb, void *cb_arg);

int dfly_device_store(int handle, struct dfly_key *key, struct dfly_value *value,
		      df_dev_io_completion_cb cb, void *cb_arg);
int dfly_device_retrieve(int handle, struct dfly_key *key, struct dfly_value *value,
			 df_dev_io_completion_cb cb, void *cb_arg);
int dfly_device_delete(int handle, struct dfly_key *key,
		       df_dev_io_completion_cb cb, void *cb_arg);
int dfly_device_build_request(int handle, int opc,
			      struct dfly_key *key, struct dfly_value *value,
			      df_dev_io_completion_cb cb, void *cb_arg,
			      struct dfly_request **pp_req);
int dfly_device_admin_passthru(int handle, struct spdk_nvme_cmd *cmd,
			       void *buff, uint64_t nbytes,
			       df_dev_io_completion_cb cb, void *cb_arg);


void dfly_register_device(struct df_device *io_device);
void dfly_register_bdev(void);
void dfly_register_kv_pool_dev(void);
#endif //_DFLY_DEVICE_H_
