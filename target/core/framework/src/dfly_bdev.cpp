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


#define DFLY_MAX_BDEV_CPU_CORES (128)

#include "spdk/bdev.h"
#include "spdk/likely.h"

#include "dragonfly.h"

/*TEST*/
bool test_write_complete = false;
void *test_buff;
int test_fhandle = -1;
void dfly_bdev_test_io(void);
//TEST
//

typedef enum device_state_e {
	DFLY_BDEV_OPENING = 1,
	DFLY_BDEV_OPENED,
	DFLY_BDEV_CLOSING
} device_state_t;

extern struct df_device dfly_bdev_device;

typedef struct dfly_bdev_s {
	struct df_device      df_dev;
	struct spdk_bdev_desc *desc;
	struct spdk_io_channel *ch[DFLY_MAX_BDEV_CPU_CORES];
	uint64_t capacity;
	int      id;
	device_state_t state;
	pthread_mutex_t device_lock;
} dfly_bdev_t;


enum dfly_bdev_io_ddir {
	DFLY_BDEV_IO_READ = 0,
	DFLY_BDEV_IO_WRITE,
	DFLY_BDEV_IO_ADMIN,
};

void dfly_bdev_remove_io_channel_done(void *vctx);

void dfly_bdev_init_io_channel(void *vctx)
{
	dfly_bdev_t *bdev_ctx = (dfly_bdev_t *)vctx;
	uint32_t core = spdk_env_get_current_core();

	pthread_mutex_lock(&bdev_ctx->device_lock);

	if (!bdev_ctx->ch[core]) {
		bdev_ctx->ch[core] = spdk_bdev_get_io_channel(bdev_ctx->desc);
		if (!bdev_ctx->ch[core]) {
			DFLY_ERRLOG("Unable to get I/O channel for bdev.\n");
		}
	}

	pthread_mutex_unlock(&bdev_ctx->device_lock);

	return;
}

void dfly_bdev_remove_io_channel(void *vctx)
{
	dfly_bdev_t *bdev_ctx = (dfly_bdev_t *)vctx;
	uint32_t core = spdk_env_get_current_core();

	// TODO Needs to cleanup IO channel independent of device_close
	// if IO channel is also separate
	if (!bdev_ctx->ch[core]) {
		return;
	}

	pthread_mutex_lock(&bdev_ctx->device_lock);

	spdk_put_io_channel(bdev_ctx->ch[core]);

	pthread_mutex_unlock(&bdev_ctx->device_lock);

	return;
}

void dfly_bdev_init_io_channel_done(void *vctx)
{
//	DFLY_NOTICELOG("ddfly bdev channel init done\n");

	dfly_bdev_t *bdev_ctx = (dfly_bdev_t *)vctx;

	pthread_mutex_lock(&bdev_ctx->device_lock);

	if (bdev_ctx->state == DFLY_BDEV_CLOSING) {
		spdk_for_each_thread(dfly_bdev_remove_io_channel, bdev_ctx, dfly_bdev_remove_io_channel_done);
		spdk_bdev_close(bdev_ctx->desc);
	} else {
		bdev_ctx->state = DFLY_BDEV_OPENED;
	}

	pthread_mutex_unlock(&bdev_ctx->device_lock);

	if (test_fhandle != -1) {
		DFLY_NOTICELOG("Running test Code\n");
		dfly_bdev_test_io();
	}
	return;
}

void dfly_bdev_remove_io_channel_done(void *vctx)
{
	free(vctx);

	return;
}

void dfly_bdev_set_handle(void *vctx, int handle)
{
	dfly_bdev_t *ctx = (dfly_bdev_t *)vctx;

	ctx->id = handle;

	return;
}

struct df_device *dfly_bdev_open(const char *path, bool write)
{
	struct spdk_bdev *bdev  = NULL;

	dfly_bdev_t *bdev_ctx;

	int rc = 0, index;

	bdev = spdk_bdev_get_by_name(path);
	if (bdev == NULL) {
		DFLY_ERRLOG("Could not find namespace bdev '%s'\n", path);
		return NULL;
	}

	bdev_ctx = (dfly_bdev_t *)calloc(1, sizeof(dfly_bdev_t));
	if (!bdev_ctx) {
		return NULL;
	}

	pthread_mutex_init(&bdev_ctx->device_lock, NULL);

	bdev_ctx->capacity = spdk_bdev_get_num_blocks(bdev) *
			     spdk_bdev_get_block_size(bdev);

	rc = spdk_bdev_open(bdev, write, NULL, NULL, &bdev_ctx->desc);
	if (rc != 0) {
		DFLY_ERRLOG("bdev %s cannot be opened, error=%d\n",
			    spdk_bdev_get_name(bdev), rc);
		goto error;
	}

	bdev_ctx->df_dev = dfly_bdev_device;

	//Close should not be called before open returns
	bdev_ctx->state  = DFLY_BDEV_OPENING;

	//Allocate channel on each core
	//This does not wait for the completion on all cores as of now
	spdk_for_each_thread(dfly_bdev_init_io_channel, bdev_ctx, dfly_bdev_init_io_channel_done);

	return &bdev_ctx->df_dev;

error:
	if (bdev_ctx) {
		free(bdev_ctx);
	}
	return NULL;
}

void dfly_bdev_close(struct df_device *device)
{
	dfly_bdev_t *ctx = (dfly_bdev_t *)device; //TODO: Container of
	int prev_state;

	pthread_mutex_lock(&ctx->device_lock);

	prev_state = ctx->state;
	ctx->state = DFLY_BDEV_CLOSING;

	pthread_mutex_unlock(&ctx->device_lock);

	if (prev_state == DFLY_BDEV_OPENING) {
		//DFLY_NOTICELOG("Close called on device %p before open completed\n", device);
		return;
	}

	//This does not wait for the completion on all cores as of now
	spdk_for_each_thread(dfly_bdev_remove_io_channel, ctx, dfly_bdev_remove_io_channel_done);
	spdk_bdev_close(ctx->desc);

}

static void
dfly_bdev_io_completion_cb(struct spdk_bdev_io *bdev_io,
			   bool success,
			   void *cb_arg)
{
	struct dfly_request *req = (struct dfly_request *)cb_arg;
	struct df_dev_response_s resp;

	if (!success) {
		//TODO: Fail counter
	}

	resp.rc = success;

	if (req->ops.complete_req) {
		req->ops.complete_req(resp, req->req_private);
	}

	dfly_io_put_req(NULL, req);

	spdk_bdev_free_io(bdev_io);
}

int dfly_bdev_io(dfly_bdev_t *ctx, void *buff, uint64_t offset, uint64_t nbytes,
		 df_dev_io_completion_cb cb, void *cb_arg, struct spdk_nvme_cmd *cmd,
		 enum dfly_bdev_io_ddir ddir)
{
	int rc = -ENODEV;
	uint32_t io_core = spdk_env_get_current_core();

	spdk_bdev_io_completion_cb completion_cb;

	struct dfly_request *req;

	if (spdk_unlikely(!ctx->ch[io_core])) {
		assert(ctx->state != DFLY_BDEV_CLOSING);
		dfly_bdev_init_io_channel(ctx);
		if (!ctx->ch[io_core]) {
			return rc;
		}
	}

	req = dfly_io_get_req(NULL);
	assert(req);

	req->req_private = cb_arg;
	req->ops.complete_req = cb;

	if (ddir == DFLY_BDEV_IO_WRITE) {
		assert(cmd == NULL);
		rc = spdk_bdev_write(ctx->desc, ctx->ch[io_core],
				     buff, offset, nbytes,
				     dfly_bdev_io_completion_cb, req);
	} else if (ddir == DFLY_BDEV_IO_READ) {
		assert(cmd == NULL);
		rc = spdk_bdev_read(ctx->desc, ctx->ch[io_core],
				    buff, offset, nbytes,
				    dfly_bdev_io_completion_cb, req);
	} else if (ddir == DFLY_BDEV_IO_ADMIN) {
		assert(cmd);
		rc = spdk_bdev_nvme_admin_passthru(ctx->desc, ctx->ch[io_core],
						   cmd,
						   buff, nbytes,
						   dfly_bdev_io_completion_cb, req);
	} else {
		//Command not handled
		assert(0);
	}

	return rc;
}

/*
 * @brief API to get capacity of device
 *
 * @param handle :integer corresponding to open device
 *
 * @return capacity of device in bytes
 */
uint64_t dfly_bdev_getsize(struct df_device *device)
{
	dfly_bdev_t *ctx = (dfly_bdev_t *)device;

	return ctx->capacity;
}

/*
 * @brief API to read from the block device
 *
 * @param handle :integer corresponding to open device
 * @param buff   : buffer for reading the data
 * @param offset :starting byte offset. Should be a multiple of block size of device
 * @param nbytes :Number of bytes to be read. Should be a multiple of block size of device
 * @param cb     : Complation call back for async IO. Can be NULL if no post processing needed
 * @param cb_arg : Argument to be parameter for completion callback
 *
 * @return integer containing the submission status
 */
int dfly_bdev_read(struct df_device *device, void *buff, uint64_t offset, uint64_t nbytes,
		   df_dev_io_completion_cb cb, void *cb_arg)
{
	//TODO: Container of device
	return dfly_bdev_io((dfly_bdev_t *)device, buff, offset, nbytes, cb, cb_arg, NULL,
			    DFLY_BDEV_IO_READ);
}

/*
 * @brief API to write to the block device
 *
 * @param handle :integer corresponding to open device
 * @param buff   :Buffer containing data to write
 * @param offset :starting byte offset. Should be a multiple of block size of device
 * @param nbytes :Number of bytes to be read. Should be a multiple of block size of device
 * @param cb     :Complation call back for async IO. Can be NULL if no post processing needed
 * @param cb_arg :Argument to be parameter for completion callback
 *
 * @return integer containing the submission status
 */
int dfly_bdev_write(struct df_device *device, void *buff, uint64_t offset, uint64_t nbytes,
		    df_dev_io_completion_cb cb, void *cb_arg)
{
	//TODO: Container of device
	return dfly_bdev_io((dfly_bdev_t *)device, buff, offset, nbytes, cb, cb_arg, NULL,
			    DFLY_BDEV_IO_WRITE);
}

int dfly_bdev_admin_passthru(struct df_device *device, struct spdk_nvme_cmd *cmd,
			     void *buff, uint64_t nbytes,
			     df_dev_io_completion_cb cb, void *cb_arg)
{
	return dfly_bdev_io((dfly_bdev_t *)device, buff, 0, nbytes, cb, cb_arg, cmd, DFLY_BDEV_IO_ADMIN);
}

struct df_device dfly_bdev_device = {
	.type = DFLY_DEVICE_TYPE_BDEV,
	.ops = {
		.open = dfly_bdev_open,
		.close = dfly_bdev_close,
		.io_complete = NULL,
		.getsize = dfly_bdev_getsize,
		.admin_passthru = dfly_bdev_admin_passthru,
		.d = {
			.blk = {
				.read = dfly_bdev_read,
				.write = dfly_bdev_write,
			},
		},
		.set_handle = dfly_bdev_set_handle,
	},
};

void dfly_register_bdev(void)
{
	dfly_register_device(&dfly_bdev_device);
}

/** Test Code
 */
static void
dfly_bdev_test_completion_cb(struct spdk_bdev_io *bdev_io,
			     bool success,
			     void *cb_arg)
{
	if (!success) {
		//TODO: Fail counter
	}
	if (test_write_complete == true) {
		DFLY_NOTICELOG("READ BUFFER:\n%s\n", test_buff);
		DFLY_NOTICELOG("\n\n\n\nREAD BUFFER LEN:%d\n", strlen((char *)test_buff));
		spdk_bdev_free_io(bdev_io);
		dfly_io_put_buff(NULL, test_buff);
		test_buff = NULL;

		dfly_device_close(test_fhandle);
		test_fhandle = -1;
	} else {
		test_write_complete = true;
		memset(test_buff, 0, 4095);
		dfly_device_read(test_fhandle, test_buff, 0, 4096,
				 (df_dev_io_completion_cb)dfly_bdev_test_completion_cb, NULL);
		spdk_bdev_free_io(bdev_io);
	}
}

void dfly_bdev_test_io(void)
{
	test_buff = dfly_io_get_buff(NULL, 4096);

	memset(test_buff, 'C', 4095);

	dfly_device_write(test_fhandle, test_buff, 0, 4096,
			  (df_dev_io_completion_cb)dfly_bdev_test_completion_cb, NULL);
	DFLY_NOTICELOG("Issued test Write IO to block0 with 4kB\n");

}

void dfly_bdev_test(void)
{
	//Expects nvme block device in spdk config with name waltest
	test_fhandle = dfly_device_open((char *)"waltestn1", DFLY_DEVICE_TYPE_BDEV, true);

	DFLY_NOTICELOG("bdev size: %ld\n", dfly_device_getsize(test_fhandle));
}
