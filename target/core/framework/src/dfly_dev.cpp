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


#include "dragonfly.h"


struct df_device_mgr g_df_dev_mgr;

void dfly_register_device(struct df_device *io_device)
{
	struct df_device_module *dev_module;

	TAILQ_FOREACH(dev_module, &g_df_dev_mgr.dfly_dev_modules, link) {
		if (dev_module->io_device == io_device) {
			DFLY_WARNLOG("Device already registered");
			return;
		}
	}

	dev_module = (struct df_device_module *)calloc(1, sizeof(struct df_device_module));
	if (!dev_module) {
		DFLY_ERRLOG("Memory allocation for device register failed");
		assert(0);
		return;
	}

	dev_module->io_device = io_device;
	TAILQ_INSERT_TAIL(&g_df_dev_mgr.dfly_dev_modules, dev_module, link);
	return;
}

int dfly_device_alloc_handle(struct df_device *ctx)
{
	int i, ret = -1;

	pthread_mutex_lock(&g_df_dev_mgr.ctx_index_lock);

	for (i = 0; i < DFLY_MAX_HANDLES; i++) {
		if (g_df_dev_mgr.ctx[i] == NULL) {
			ret = i;
			assert(ctx);
			g_df_dev_mgr.ctx[i] = ctx;
			break;
		}
	}

	pthread_mutex_unlock(&g_df_dev_mgr.ctx_index_lock);

	return ret;
}

void dfly_device_release_handle(int handle)
{

	assert(handle >= 0);
	assert(handle < DFLY_MAX_HANDLES);

	if (handle < 0 || handle > DFLY_MAX_HANDLES) {
		DFLY_ERRLOG("Invalid handle provided\n");
	}

	pthread_mutex_lock(&g_df_dev_mgr.ctx_index_lock);

	g_df_dev_mgr.ctx[handle] = NULL;

	pthread_mutex_unlock(&g_df_dev_mgr.ctx_index_lock);
}


static inline struct df_device *dfly_dev_get_device(int handle)
{
	assert(handle >= 0);
	assert(handle < DFLY_MAX_HANDLES);

	assert(g_df_dev_mgr.ctx[handle]);

	return g_df_dev_mgr.ctx[handle];
}

int dfly_dev_init(void)
{

	g_df_dev_mgr.dfly_dev_modules = TAILQ_HEAD_INITIALIZER(g_df_dev_mgr.dfly_dev_modules),
	pthread_mutex_init(&g_df_dev_mgr.ctx_index_lock, NULL);

	dfly_register_bdev();

	dfly_register_kv_pool_dev();

	/*TODO initialize list of all device types */
}

bool dfly_bdev_write_cached(const char *path)
{
	struct spdk_bdev *bdev = spdk_bdev_get_by_name(path);
	return spdk_bdev_has_write_cache(bdev);
}

int dfly_device_open(char *path, devtype_t type, bool write)
{
	struct df_device_module *dev_module;
	bool found = false;

	struct df_device *device_ctx = NULL;
	int handle = -1;

	TAILQ_FOREACH(dev_module, &g_df_dev_mgr.dfly_dev_modules, link) {
		if (dev_module->io_device->type == type) {
			found = true;
			break;
		}
	}

	if (found) {
		device_ctx = dev_module->io_device->ops.open(path, write);
	}

	if (device_ctx) {
		handle = dfly_device_alloc_handle(device_ctx);
	}

	return handle;
}

void dfly_device_close(int handle)
{
	struct df_device *device = dfly_dev_get_device(handle);

	device->ops.close(device);
	dfly_device_release_handle(handle);

	return;
}

uint64_t dfly_device_getsize(int handle)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.getsize(device);
}

/*
 * Block device API
 */

int dfly_device_write(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		      df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	assert(device->type == DFLY_DEVICE_TYPE_BDEV);

	return device->ops.d.blk.write(device, buff, offset, nbytes, cb, cb_arg);
}

int dfly_device_read(int handle, void *buff, uint64_t offset, uint64_t nbytes,
		     df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	assert(device->type == DFLY_DEVICE_TYPE_BDEV);

	return device->ops.d.blk.read(device, buff, offset, nbytes, cb, cb_arg);
}

/*
 * Key Value API
 */



void *dfly_device_create_iter_info(int handle, struct dfly_request *req)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.iter_info_setup(device, req);
}

int dfly_device_iter_open(int handle, void *iter_info,
			  df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.iter_open(device, iter_info, cb, cb_arg);
}

int dfly_device_iter_close(int handle, void *iter_info,
			   df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.iter_close(device, iter_info, cb, cb_arg);
}

int dfly_device_iter_read(int handle, void *iter_info,
			  df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.iter_read(device, iter_info, cb, cb_arg);
}


int dfly_device_store(int handle, struct dfly_key *key, struct dfly_value *value,
		      df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.store(device, key, value, cb, cb_arg);
}

int dfly_device_retrieve(int handle, struct dfly_key *key, struct dfly_value *value,
			 df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.retrieve(device, key, value, cb, cb_arg);
}

int dfly_device_delete(int handle, struct dfly_key *key,
		       df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.key_delete(device, key, cb, cb_arg);
}

int dfly_device_build_request(int handle, int opc,
			      struct dfly_key *key, struct dfly_value *value,
			      df_dev_io_completion_cb cb, void *cb_arg,
			      struct dfly_request **pp_req)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.d.kv.build_request(device, opc, key, value, cb, cb_arg, pp_req);
}

int dfly_device_exists(int handle, void *key, uint32_t key_len, uint32_t nkeys,
		       void *buffer, uint32_t buffer_len,
		       df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	//return device->ops.d.kv.exist(device, key, key_len, nkeys, buffer, buffer_len, cb, cb_arg);
	return 0; //TODO
}

int dfly_device_iterate(int handle, void *prefix, uint32_t prefix_len,
			void *buffer, uint32_t buffer_len,
			df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	//return device->ops.d.kv.iterate(device, prefix, prefix_len, buffer, buffer_len, cb, cb_arg);
	return 0; //TODO
}

/*
 * Common NVMe Passthru API
 */
int dfly_device_admin_passthru(int handle, struct spdk_nvme_cmd *cmd,
			       void *buff, uint64_t nbytes,
			       df_dev_io_completion_cb cb, void *cb_arg)
{
	struct df_device *device = dfly_dev_get_device(handle);

	return device->ops.admin_passthru(device, cmd, buff, nbytes, cb, cb_arg);
}

