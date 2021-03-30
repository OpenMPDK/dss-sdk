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


#include "spdk/stdinc.h"

#include "spdk/log.h"
#include "spdk/rpc.h"
#include "spdk/util.h"
#include "spdk/nvmf.h"

#include "dragonfly.h"
#include "base64.h"

#include "version.h"

struct dfly_migrate {
	int32_t src_id;
	int32_t dst_id;
	char *prefix;
};

static const struct spdk_json_object_decoder dfly_migrate_decoders[] = {
	{"src_id", offsetof(struct dfly_migrate, src_id), spdk_json_decode_int32},
	{"dst_id", offsetof(struct dfly_migrate, dst_id), spdk_json_decode_int32},
	{"prefix", offsetof(struct dfly_migrate, prefix), spdk_json_decode_string, true},
};

static void
free_dragonfly_migrate(struct dfly_migrate *req)
{
	free(req->prefix);
}

static void
dragonfly_dump_batch_id(struct spdk_json_write_ctx *w, int32_t batch_id)
{
	spdk_json_write_object_begin(w);
	spdk_json_write_name(w, "batch_id");
	spdk_json_write_int32(w, batch_id);
	spdk_json_write_object_end(w);
}

static void
dragonfly_migrate_data(struct spdk_jsonrpc_request *request,
		       const struct spdk_json_val *params)
{
	struct spdk_json_write_ctx *w;
	struct dfly_migrate req = {};

	if (spdk_json_decode_object(params, dfly_migrate_decoders,
				    SPDK_COUNTOF(dfly_migrate_decoders),
				    &req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		goto invalid;
	}

	DFLY_NOTICELOG("Source:      %d\n", req.src_id);
	DFLY_NOTICELOG("Destination: %d\n", req.dst_id);
	if (req.prefix) {
		DFLY_NOTICELOG("Prefix:      %s\n", req.prefix);
	}

	/* Validate migrate data parameters
	 */

	/* Call Dragonfly specific APIs
	 */
	int32_t batch_id = 123;

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_array_begin(w);
	dragonfly_dump_batch_id(w, batch_id);
	spdk_json_write_array_end(w);
	spdk_jsonrpc_end_result(request, w);

	free_dragonfly_migrate(&req);

	return;

invalid:
	spdk_jsonrpc_send_error_response(request, SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid parameters");
	free_dragonfly_migrate(&req);
}
SPDK_RPC_REGISTER("dfly_migrate_data", dragonfly_migrate_data, SPDK_RPC_RUNTIME)

static void
dragonfly_oss_version_info(struct spdk_jsonrpc_request *request,
			   const struct spdk_json_val *params)
{
	struct spdk_json_write_ctx *w;

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_array_begin(w);

	spdk_json_write_object_begin(w);
	spdk_json_write_name(w, "OSS_TARGET_VER");
	spdk_json_write_string(w, OSS_TARGET_VER);
	spdk_json_write_name(w, "OSS_TARGET_GIT_VER");
	spdk_json_write_string(w, OSS_TARGET_GIT_VER);
	spdk_json_write_object_end(w);

	spdk_json_write_array_end(w);

	spdk_jsonrpc_end_result(request, w);
}
SPDK_RPC_REGISTER("dfly_oss_version_info", dragonfly_oss_version_info, SPDK_RPC_RUNTIME)

struct dfly_status {
	int32_t batch_id;
};

static const struct spdk_json_object_decoder dfly_status_decoders[] = {
	{"batch_id", offsetof(struct dfly_status, batch_id), spdk_json_decode_int32},
};


static void
dfly_dump_disk_state(struct spdk_json_write_ctx *w)
{
	spdk_json_write_name(w, "TARGET_DISK");
	spdk_json_write_object_begin(w);

	dict_t *disk_table = g_dragonfly->disk_stat_table;
	dict_t *current_item, *tmp;
	// TODO: Need to check the HASH_iter work mechanism when removing the ns from the table
	HASH_ITER(hh, disk_table, current_item, tmp) {
		spdk_json_write_name(w, current_item->key);
		spdk_json_write_object_begin(w);
		spdk_json_write_name(w, "status");
		spdk_json_write_int32(w, current_item->value);
		//spdk_json_write_int32(w, ptr->value);
		spdk_json_write_name(w, "message");
		spdk_json_write_string(w, current_item->message);
		spdk_json_write_object_end(w);
	}
	spdk_json_write_object_end(w);
}

static void
dfly_dump_subsystem_state(struct spdk_json_write_ctx *w)
{
	struct spdk_nvmf_subsystem *subsystem;

	spdk_json_write_name(w, "SUBSYSTEM");
	spdk_json_write_object_begin(w);

	subsystem = spdk_nvmf_subsystem_get_first(dfly_get_g_nvmf_tgt());
	while (subsystem) {
		spdk_json_write_name(w, spdk_nvmf_subsystem_get_nqn(subsystem));
		spdk_json_write_object_begin(w);
		spdk_json_write_name(w, "status");

		// TODO:
		// Change 0 to a functionality when the subsystem dead mechanism is decided
		// Change message to corresponding message when decided
		spdk_json_write_int32(w, 0);
		spdk_json_write_name(w, "message");
		spdk_json_write_string(w, "The subsystem is alive");
		spdk_json_write_object_end(w);
		subsystem = spdk_nvmf_subsystem_get_next(subsystem);
	}

	spdk_json_write_object_end(w);
}

static void
dfly_dump_nic_state(struct spdk_json_write_ctx *w)
{
	spdk_json_write_name(w, "TARGET_NIC");
	spdk_json_write_object_begin(w);

	spdk_json_write_name(w, "nic_unique_serial");
	spdk_json_write_object_begin(w);
	spdk_json_write_name(w, "status");

	// TODO:
	// Change 0 to a functionality when the subsystem dead mechanism is decided
	// Change message to corresponding message when decided
	spdk_json_write_int32(w, 0);
	spdk_json_write_name(w, "message");
	spdk_json_write_string(w, "sample message");
	spdk_json_write_object_end(w);

	spdk_json_write_object_end(w);
}

static void
dfly_dump_timestamp(struct spdk_json_write_ctx *w)
{

	spdk_json_write_name(w, "timestamp");
	time_t current_time;

	/* Obtain current time. */
	current_time = time(NULL);

	if (current_time == ((time_t) -1)) {
		(void) fprintf(stderr, "Failure to obtain the current time.\n");
		exit(1);
	}

	spdk_json_write_uint64(w, current_time);
}

void
dfly_dump_status_info(struct spdk_json_write_ctx *w)
{
	spdk_json_write_name(w, "status");
	spdk_json_write_object_begin(w);
	dfly_dump_timestamp(w);
	dfly_dump_disk_state(w);
	dfly_dump_subsystem_state(w);
	dfly_dump_nic_state(w);
	spdk_json_write_object_end(w);
}



static void
dragonfly_dump_status(struct spdk_json_write_ctx *w, int32_t status)
{
	spdk_json_write_object_begin(w);
	spdk_json_write_name(w, "status");
	spdk_json_write_int32(w, status);
	spdk_json_write_object_end(w);
}

static void
dragonfly_status(struct spdk_jsonrpc_request *request,
		 const struct spdk_json_val *params)
{
	struct spdk_json_write_ctx *w;
	struct dfly_status req = {};

	if (spdk_json_decode_object(params, dfly_status_decoders,
				    SPDK_COUNTOF(dfly_status_decoders),
				    &req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		goto invalid;
	}

	/* Validate process status parameters
	 */

	/* Call Dragonfly specific APIs
	 */
	DFLY_NOTICELOG("Retrieve status for batch_id: %d\n", req.batch_id);
	int32_t status = 0;

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_array_begin(w);
	dragonfly_dump_status(w, status);
	spdk_json_write_array_end(w);
	spdk_jsonrpc_end_result(request, w);

	return;

invalid:
	spdk_jsonrpc_send_error_response(request, SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid parameters");
}
SPDK_RPC_REGISTER("dfly_get_status", dragonfly_status, SPDK_RPC_RUNTIME)

struct dfly_nvme_pt {
	uint32_t opcode; //__u8
	uint32_t flags;  //__u8
	uint32_t rsvd1;  //__u16
	uint32_t nsid;
	uint32_t cdw2;
	uint32_t cdw3;
	uint32_t data_len;
	uint32_t cdw10;
	uint32_t cdw11;
	uint32_t cdw12;
	uint32_t cdw13;
	uint32_t cdw14;
	uint32_t cdw15;
	uint32_t timeout_ms;
	uint32_t result;
	char *device_name;
};

struct dfly_passthru_ctx {
	struct spdk_nvme_cmd cmd;
	struct spdk_jsonrpc_request *request;
	int bdev;
	char *data;
	size_t data_len;
};

static const struct spdk_json_object_decoder dfly_admin_cmd_decoders[] = {
	{"opcode", offsetof(struct dfly_nvme_pt, opcode), spdk_json_decode_uint32},
	//{"flags", offsetof(struct dfly_nvme_pt, flags), spdk_json_decode_uint32},
	{"rsvd1", offsetof(struct dfly_nvme_pt, rsvd1), spdk_json_decode_uint32, true},
	{"nsid", offsetof(struct dfly_nvme_pt, nsid), spdk_json_decode_uint32},
	//{"cdw2", offsetof(struct dfly_nvme_pt, cdw2), spdk_json_decode_uint32},
	//{"cdw3", offsetof(struct dfly_nvme_pt, cdw3), spdk_json_decode_uint32},
	{"data_len", offsetof(struct dfly_nvme_pt, data_len), spdk_json_decode_uint32},
	//{"metadata_len", offsetof(struct dfly_nvme_pt, metadata_len), spdk_json_decode_uint32},
	{"cdw10", offsetof(struct dfly_nvme_pt, cdw10), spdk_json_decode_uint32},
	{"cdw11", offsetof(struct dfly_nvme_pt, cdw11), spdk_json_decode_uint32, true},
	{"cdw12", offsetof(struct dfly_nvme_pt, cdw12), spdk_json_decode_uint32, true},
	{"cdw13", offsetof(struct dfly_nvme_pt, cdw13), spdk_json_decode_uint32, true},
	{"cdw14", offsetof(struct dfly_nvme_pt, cdw14), spdk_json_decode_uint32, true},
	{"cdw15", offsetof(struct dfly_nvme_pt, cdw15), spdk_json_decode_uint32, true},
	//{"timeout_ms", offsetof(struct dfly_nvme_pt, timeout_ms), spdk_json_decode_uint32},
	//{"result", offsetof(struct dfly_nvme_pt, result), spdk_json_decode_uint32},
	{"device_name", offsetof(struct dfly_nvme_pt, device_name), spdk_json_decode_string},
};

static struct dfly_passthru_ctx *
dragonfly_init_passthru_ctx(struct dfly_nvme_pt *req,
			    struct spdk_jsonrpc_request *request)
{
	struct dfly_passthru_ctx *ctx = NULL;
	ctx = calloc(1, sizeof(struct dfly_passthru_ctx));
	if (ctx == NULL) {
		return NULL;
	}

	ctx->request = request;
	ctx->data_len = req->data_len;

	ctx->cmd.opc = (uint8_t)(req->opcode);
	//ctx->cmd.flags = (uint8_t)(req->flags);
	ctx->cmd.rsvd1 = (uint16_t)(req->rsvd1);
	ctx->cmd.nsid = req->nsid;
	//ctx->cmd.cdw2 = req->cdw2;
	//ctx->cmd.cdw3 = req->cdw3;
	ctx->cmd.cdw10 = req->cdw10;
	ctx->cmd.cdw11 = req->cdw11;
	ctx->cmd.cdw12 = req->cdw12;
	ctx->cmd.cdw13 = req->cdw13;
	ctx->cmd.cdw14 = req->cdw14;
	ctx->cmd.cdw15 = req->cdw15;
	//ctx->cmd.timeout_ms = req->timeout_ms;
	//ctx->cmd.result = req->result;

	return ctx;
}

static void
free_dragonfly_passthru(struct dfly_nvme_pt *req)
{
	free(req->device_name);
}

static void
passthru_cb(struct df_dev_response_s resp, void *cb_arg)
{
	struct dfly_passthru_ctx *ctx = (struct dfly_passthru_ctx *)cb_arg;
	struct spdk_jsonrpc_request *request = ctx->request;
	struct spdk_json_write_ctx *w;
	unsigned char *payload = NULL;
	size_t payload_len = 0;
	int32_t err = 0;
	char msg[1024];

	if (!resp.rc) {
		DFLY_ERRLOG("dfly_device_passthru failed in fn passthru_cb\n");
		snprintf(msg, sizeof(msg), "dfly_device_passthru failed in fn passthru_cb");
		err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
		goto cleanup;
	}

	/* Encode data to Base64
	 */
	payload = base64_encode(ctx->data, ctx->data_len, &payload_len);
	if (payload == NULL) {
		snprintf(msg, sizeof(msg), "Failed encode payload");
		err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
		goto cleanup;
	}

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		snprintf(msg, sizeof(msg), "Failed to initialize JSON RPC response");
		err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
		goto clean_encode;
	}

	spdk_json_write_array_begin(w);
	spdk_json_write_object_begin(w);
	spdk_json_write_name(w, "payload");
	spdk_json_write_string_raw(w, payload, payload_len);
	spdk_json_write_name(w, "payload_len");
	spdk_json_write_int32(w, payload_len);
	spdk_json_write_object_end(w);
	spdk_json_write_array_end(w);
	spdk_jsonrpc_end_result(request, w);

clean_encode:
	if (payload) {
		free(payload);
	}

cleanup:
	spdk_dma_free(ctx->data);
	dfly_device_close(ctx->bdev);
	free(ctx);

	if (err != 0) {
		spdk_jsonrpc_send_error_response(request, err, msg);
	}
}

static void
dragonfly_nvme_passthru(struct spdk_jsonrpc_request *request,
			const struct spdk_json_val *params)
{
	struct dfly_nvme_pt req = { 0 };
	struct dfly_passthru_ctx *ctx = NULL;
	int bdev = -1;
	char msg[1024];
	int32_t err = 0;
	int32_t rc = 0;

	if (spdk_json_decode_object(params, dfly_admin_cmd_decoders,
				    SPDK_COUNTOF(dfly_admin_cmd_decoders),
				    &req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		snprintf(msg, sizeof(msg), "Invalid parameters");
		err = SPDK_JSONRPC_ERROR_INVALID_PARAMS;
		goto invalid;
	}

	//DFLY_NOTICELOG("NVMe admin cmd passthru opcode: %d\n", req.opcode);
	if (req.opcode != 0x02 && req.opcode != 0x06) {
		DFLY_ERRLOG("dfly_nvme_passthru only supports opcode of 0x02 and 0x06\n");
		snprintf(msg, sizeof(msg), "Invalid opcode (0x02 and 0x06 only)");
		err = SPDK_JSONRPC_ERROR_INVALID_PARAMS;
		goto invalid;
	}

	ctx = dragonfly_init_passthru_ctx(&req, request);
	if (ctx == NULL) {
		snprintf(msg, sizeof(msg), "Failed to allocate passthru context");
		err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
		goto invalid;
	}

	/* Retrieve SPDK bdev and channel
	 */
	if ((bdev = dfly_device_open(req.device_name, DFLY_DEVICE_TYPE_BDEV, false)) == -1) {
		snprintf(msg, sizeof(msg), "bdev %s were not found", req.device_name);
		err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
		goto clean_ctx;
	}
	ctx->bdev = bdev;

	/* Allocate buffer for NVMe admin cmd
	 */
	if (ctx->data_len) {
		ctx->data = spdk_dma_zmalloc(ctx->data_len, 4096, NULL);
		if (ctx->data == NULL) {
			DFLY_ERRLOG("Failed to allocate data buffer for passthru cmd\n");
			snprintf(msg, sizeof(msg), "Failed to allocate data buffer for passthru cmd");
			err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
			goto clean_device;
		}
	}

	/* Call Dragonfly passthru API
	 */
	rc = dfly_device_admin_passthru(bdev, &ctx->cmd,
					(void *)ctx->data, ctx->data_len,
					passthru_cb, ctx);
	if (rc) {
		DFLY_ERRLOG("dfly_device_passthru failed - %d\n", rc);
		snprintf(msg, sizeof(msg), "dfly_device_passthru failed - %d", rc);
		err = SPDK_JSONRPC_ERROR_INTERNAL_ERROR;
	}

	if (err != 0) {
		if (ctx && ctx->data) {
			spdk_dma_free(ctx->data);
		}

clean_device:
		if (bdev != -1) {
			dfly_device_close(bdev);
		}

clean_ctx:
		if (ctx) {
			free(ctx);
		}

invalid:
		spdk_jsonrpc_send_error_response(request, err, msg);
	}

	free_dragonfly_passthru(&req);
}
SPDK_RPC_REGISTER("dfly_nvme_passthru", dragonfly_nvme_passthru, SPDK_RPC_RUNTIME)

#ifdef SPDK_CONFIG_SAMSUNG_COUNTERS
struct dfly_statistics {
	char *counter;
};

static void
free_rpc_statistics(struct dfly_statistics *r)
{
	free(r->counter);
}

static const struct spdk_json_object_decoder dfly_statistics_decoders[] = {
	{"counter", offsetof(struct dfly_statistics, counter), spdk_json_decode_string, true},
};

static void
dragonfly_dump_statistics(struct spdk_json_write_ctx *w, long long *counter)
{
	// TODO: Retrieve from ustats
	/*if (counter == NULL) {
	    spdk_json_write_name(w, "io_count");
	    spdk_json_write_int64(w, ATOMIC_READ(g_dfly_counters.io_count));
	    spdk_json_write_name(w, "put_count");
	    spdk_json_write_int64(w, ATOMIC_READ(g_dfly_counters.put_count));
	    spdk_json_write_name(w, "get_count");
	    spdk_json_write_int64(w, ATOMIC_READ(g_dfly_counters.get_count));
	    spdk_json_write_name(w, "del_count");
	    spdk_json_write_int64(w, ATOMIC_READ(g_dfly_counters.del_count));
	    spdk_json_write_name(w, "exist_count");
	    spdk_json_write_int64(w, ATOMIC_READ(g_dfly_counters.exist_count));
	    spdk_json_write_name(w, "iter_count");
	    spdk_json_write_int64(w, ATOMIC_READ(g_dfly_counters.iter_count));
	} else {
	    spdk_json_write_int64(w, ATOMIC_READ((*counter)));
	}*/
	spdk_json_write_name(w, "timestamp");
	spdk_json_write_int64(w, dfly_counters_timestamp());
}

static void
dragonfly_statistics(struct spdk_jsonrpc_request *request,
		     const struct spdk_json_val *params)
{
	struct spdk_json_write_ctx *w;
	struct dfly_statistics req = {};
	long long *counter = NULL;

	if (spdk_json_decode_object(params, dfly_statistics_decoders,
				    SPDK_COUNTOF(dfly_statistics_decoders),
				    &req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		goto invalid;
	}

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_array_begin(w);
	spdk_json_write_object_begin(w);

	/*if (strcmp(req.counter, "io_count") == 0) {
	    spdk_json_write_name(w, "io_count");
	    counter = &g_dfly_counters.io_count;
	} else if (strcmp(req.counter, "put_count") == 0) {
	    spdk_json_write_name(w, "put_count");
	    counter = &g_dfly_counters.put_count;
	} else if (strcmp(req.counter, "get_count") == 0) {
	    spdk_json_write_name(w, "get_count");
	    counter = &g_dfly_counters.get_count;
	} else if (strcmp(req.counter, "del_count") == 0) {
	    spdk_json_write_name(w, "del_count");
	    counter = &g_dfly_counters.del_count;
	} else if (strcmp(req.counter, "exist_count") == 0) {
	    spdk_json_write_name(w, "exist_count");
	    counter = &g_dfly_counters.exist_count;
	} else if (strcmp(req.counter, "iter_count") == 0) {
	    spdk_json_write_name(w, "iter_count");
	    counter = &g_dfly_counters.iter_count;
	}*/

	dragonfly_dump_statistics(w, counter);

	spdk_json_write_object_end(w);
	spdk_json_write_array_end(w);

	spdk_jsonrpc_end_result(request, w);

	free_rpc_statistics(&req);

	return;

invalid:
	spdk_jsonrpc_send_error_response(request, SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid parameters");
	free_rpc_statistics(&req);
}
SPDK_RPC_REGISTER("dfly_get_statistics", dragonfly_statistics, SPDK_RPC_RUNTIME)

#endif // SPDK_CONFIG_SAMSUNG_COUNTERS

struct dss_rpc_lat_profile_req_s {
	char *nqn;
	uint16_t cntlid;
	uint16_t qid;
};

struct dss_rpc_lat_profile_resp_entry_s {
	uint32_t percentile;
	uint32_t latency;
};

static const struct spdk_json_object_decoder dss_rpc_lat_prof_decoders[] = {
	{"nqn", offsetof(struct dss_rpc_lat_profile_req_s, nqn), spdk_json_decode_string},
	{"cid", offsetof(struct dss_rpc_lat_profile_req_s, cntlid), spdk_json_decode_uint16},
	{"qid", offsetof(struct dss_rpc_lat_profile_req_s, qid), spdk_json_decode_uint16},
};

void free_rpc_latency_profile(struct dss_rpc_lat_profile_req_s *req)
{
	free(req->nqn);
}

static void dss_rpc_get_latency_profile(struct spdk_jsonrpc_request *request,
		const struct spdk_json_val *params)
{
	struct spdk_nvmf_subsystem *subsystem;
	dfly_ctrl_t *ctrlr;
	struct spdk_json_write_ctx *w;

	struct dfly_qpair_s *dqpair;
	struct dss_rpc_lat_profile_req_s req = {};

	struct dss_lat_prof_arr *result_profile = NULL;

	char percentile_str[8];
	int i;

	if (spdk_json_decode_object(params, dss_rpc_lat_prof_decoders,
				    SPDK_COUNTOF(dss_rpc_lat_prof_decoders),
				    &req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		goto invalid;
	}

	subsystem = spdk_nvmf_tgt_find_subsystem(dfly_get_g_nvmf_tgt(), req.nqn);
	if (!subsystem) {
		DFLY_ERRLOG("Subsystem not found\n");
		goto invalid;
	}

	ctrlr = df_get_ctrl(dfly_get_nvmf_ssid(subsystem), req.cntlid);
	if(!ctrlr) {
		//TODO: find aggregate for cntlid -1
		DFLY_ERRLOG("Controller not found\n");
		goto invalid;
	}

	dqpair = df_get_dqpair(ctrlr, req.qid);
	if(dqpair) {
		if(dss_lat_get_percentile(dqpair->lat_ctx, &result_profile)) {
			DFLY_ERRLOG( "No stats available\n");
			goto invalid;
		}
	} else {
		//TODO: find aggregate for qid -1
		DFLY_ERRLOG("qpair not found\n");
		goto invalid;
	}

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_object_begin(w);

	spdk_json_write_name(w, spdk_nvmf_subsystem_get_nqn(subsystem));

	spdk_json_write_object_begin(w);//Begin subsysresult
	spdk_json_write_name(w, "latency profile");
	spdk_json_write_object_begin(w);//	begin latency profile

	for(i=0; i< result_profile->n_part;i++) {
		sprintf(percentile_str, "%d", result_profile->prof[i].pVal);
		spdk_json_write_name(w, percentile_str);
		spdk_json_write_int32(w, result_profile->prof[i].pLat);
	}

	spdk_json_write_object_end(w);// end latency profile
	spdk_json_write_object_end(w);//End Subsys result
	spdk_json_write_object_end(w);

	spdk_jsonrpc_end_result(request, w);
	free_rpc_latency_profile(&req);
	return;

invalid:
	spdk_jsonrpc_send_error_response(request, SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid Parameters");
	free_rpc_latency_profile(&req);
	return;
}
SPDK_RPC_REGISTER("dss_get_latency_profile", dss_rpc_get_latency_profile, SPDK_RPC_RUNTIME)

struct dss_rpc_reset_ustat_counters_req_s {
	char *nqn;
};

static const struct spdk_json_object_decoder dss_rpc_reset_ustat_counters_decoders[] = {
	{"nqn", offsetof(struct dss_rpc_reset_ustat_counters_req_s, nqn), spdk_json_decode_string},
};

void free_rpc_reset_ustat_counters(struct dss_rpc_reset_ustat_counters_req_s *req)
{
	free(req->nqn);
}

static void dss_rpc_reset_ustat_counters(struct spdk_jsonrpc_request *request,
		const struct spdk_json_val *params)
{
	struct spdk_nvmf_subsystem *subsystem;
	struct dfly_subsystem *df_subsys;
	struct dss_rpc_reset_ustat_counters_req_s req = {};
	struct spdk_json_write_ctx *w;

	if (spdk_json_decode_object(params, dss_rpc_reset_ustat_counters_decoders,
				    SPDK_COUNTOF(dss_rpc_reset_ustat_counters_decoders),
				    &req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		goto invalid;
	}

	subsystem = spdk_nvmf_tgt_find_subsystem(g_spdk_nvmf_tgt, req.nqn);
	if (!subsystem) {
		DFLY_ERRLOG("Subsystem not found\n");
		goto invalid;
	}

	df_subsys = dfly_get_subsystem_no_lock(dfly_get_nvmf_ssid(subsystem));

	if(df_subsys && df_subsys->initialized == true) {
		dfly_counters_reset(df_subsys);
	} else {
		DFLY_ERRLOG("Subsystem not initialized\n");
		goto invalid;
	}

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_object_begin(w);

	spdk_json_write_name(w, "reset_done");
	spdk_json_write_bool(w, true);

	spdk_json_write_object_end(w);

	spdk_jsonrpc_end_result(request, w);
	free_rpc_latency_profile(&req);
	return;

invalid:
	spdk_jsonrpc_send_error_response(request, SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid Parameters");
	free_rpc_reset_ustat_counters(&req);
	return;
}
SPDK_RPC_REGISTER("dss_reset_ustat_counters", dss_rpc_reset_ustat_counters, SPDK_RPC_RUNTIME)

struct dss_rpc_nqn_req_s {
	char *nqn;
	bool get_status;
};

static const struct spdk_json_object_decoder dss_rpc_nqn_decoder[] = {
	{"nqn", offsetof(struct dss_rpc_nqn_req_s, nqn), spdk_json_decode_string},
	{"get_status", offsetof(struct dss_rpc_nqn_req_s, get_status), spdk_json_decode_bool, true},
};

static  struct dfly_subsystem * dss_rpc_decode_nqn_param(const struct spdk_json_val *params,
					struct dss_rpc_nqn_req_s *nqn_req)
{
	struct spdk_nvmf_subsystem *subsystem;
	struct dfly_subsystem *df_subsys;

	if (spdk_json_decode_object(params, dss_rpc_nqn_decoder,
				    SPDK_COUNTOF(dss_rpc_nqn_decoder),
				    nqn_req)) {
		DFLY_ERRLOG("spdk_json_decode_object failed\n");
		return NULL;
	}

	subsystem = spdk_nvmf_tgt_find_subsystem(g_spdk_nvmf_tgt, nqn_req->nqn);
	if (!subsystem) {
		DFLY_ERRLOG("Subsystem not found\n");
		return NULL;
	}

	df_subsys = dfly_get_subsystem_no_lock(dfly_get_nvmf_ssid(subsystem));

	if(df_subsys && df_subsys->initialized == true) {
		return df_subsys;
	} else {
		DFLY_ERRLOG("Subsystem not initialized\n");
		return NULL;
	}

}

void free_rpc_nqn_req(struct dss_rpc_nqn_req_s *req)
{
	free(req->nqn);
}


static void dss_rpc_rdb_compact(struct spdk_jsonrpc_request *request,
		const struct spdk_json_val *params)
{
	struct dss_rpc_nqn_req_s req = {};
	struct dfly_subsystem *df_subsys;
	struct spdk_json_write_ctx *w;
	int i;
	char *result_str;
	bool compact_in_progress = false;

	req.get_status = false;

	if((df_subsys = dss_rpc_decode_nqn_param(params, &req)) == NULL) {
		goto invalid;
	}

	pthread_mutex_lock(&df_subsys->subsys_lock);
	for(i=0; i < df_subsys->num_io_devices; i++) {
		if(df_subsys->devices[i].rdb_handle->compaction_in_progress == true) {
			compact_in_progress = true;
			break;
		}
	}
	pthread_mutex_unlock(&df_subsys->subsys_lock);//Release lock

	if(req.get_status) {
		if(compact_in_progress) {
			result_str = "IN PROGRESS";
		} else {
			result_str = "IDLE";
		}
	} else {
		if(!compact_in_progress) {
			for(i=0; i < df_subsys->num_io_devices; i++) {
				dss_rocksdb_compaction(&df_subsys->devices[i]);
			}
			result_str = "STARTED";
		} else {
			result_str = "RUNNING";
		}
	}

	w = spdk_jsonrpc_begin_result(request);
	if (w == NULL) {
		return;
	}

	spdk_json_write_object_begin(w);

	spdk_json_write_name(w, "result");
	spdk_json_write_string(w, result_str);

	spdk_json_write_object_end(w);

	spdk_jsonrpc_end_result(request, w);
	free_rpc_latency_profile(&req);
	return;

invalid:
	spdk_jsonrpc_send_error_response(request, SPDK_JSONRPC_ERROR_INVALID_PARAMS, "Invalid Parameters");
	free_rpc_nqn_req(&req);
}
SPDK_RPC_REGISTER("dss_rdb_compact", dss_rpc_rdb_compact, SPDK_RPC_RUNTIME)
