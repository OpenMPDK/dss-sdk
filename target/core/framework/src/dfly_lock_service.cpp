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

# include "dragonfly.h"
#include <unordered_map>

struct dfly_lock_key_s {
	uint64_t key_p1;
	uint64_t key_p2;
};

typedef struct lock_request_s {
	/* dword 0 */
	uint16_t opc	:  8;	/* opcode */
	uint16_t fuse	:  2;	/* fused operation */
	uint16_t rsvd1	:  4;
	uint16_t psdt	:  2;
	uint16_t cid;		/* command identifier */

	/* dword 1 */
	uint32_t nsid;		/* namespace identifier */

	/* dword 2-3 */
	uint64_t uuid;

	/* dword 4-5 */
	uint64_t mptr;		/* metadata pointer - Not Used*/

	/* dword 6-9: data pointer */
	uint64_t dptr1;//Not Used
	uint64_t dptr2;//Not Used

	/* dword 10-15 */
	uint32_t lock_duration;		/* in seconds */
	union {
		struct lock_opt_s {
			uint32_t keylen: 8;
			uint32_t priority: 2;
			uint32_t write_lock: 1;
			uint32_t blocking: 1; //Not used for unlock
		} lock;
		uint32_t cdw11;		/* command-specific */
	} opt;
	struct dfly_lock_key_s key;
} lock_request_t;

typedef std::unordered_map<uint64_t, bool> htable_u64_t;

typedef struct lock_object_s {
	struct dfly_lock_key_s key;
	uint8_t  klen;
	uint16_t rcount;
	uint16_t wlock: 1;
	uint64_t expire_tick;
	htable_u64_t *uuid_ht;
	TAILQ_HEAD(, dfly_request) pending_lock_reqs;
} lock_object_t;

typedef std::unordered_map<std::string, lock_object_t *> htable_t;

#define LOBJ_MPOOL_COUNT (1024 * 1024)
struct lock_service_subsys_s {
	uint32_t id;
	htable_t *active_locks;
	dfly_mempool *lobj_mp;
};


/**
 * Hash Bitmap APIs
 */

/**
 * Return Value:
 *     true: Successfully inserted
 *     false: u64 value already exists in the table
 */
bool dfly_u64ht_insert(void *table, uint64_t value)
{
	htable_u64_t *uuid_table = (htable_u64_t *)table;
	htable_u64_t::const_iterator uuid_iter;

	uuid_iter =  uuid_table->find(value);

	if (uuid_iter == uuid_table->end()) {
		uuid_table->emplace(value, true);
		return true;
	} else {
		DFLY_ASSERT(uuid_iter->second);
		return false;
	}

}


/**
 * Return Value:
 *     true: Successfully deleted
 *     false: u64 value not in the table
 */
bool dfly_u64ht_delete(void *table, uint64_t value)
{
	htable_u64_t *uuid_table = (htable_u64_t *)table;
	htable_u64_t::const_iterator uuid_iter;

	uuid_iter =  uuid_table->find(value);

	if (uuid_iter == uuid_table->end()) {
		return false;
	} else {
		DFLY_ASSERT(uuid_iter->second);
		uuid_table->erase(value);
		return true;
	}

}

/**
 * End Hash Bitmap APIs
 */

/**
 * Hash APIs
 */

bool dfly_ht_insert(void *table, char *index_ptr, uint32_t len, void *opaque_ptr)
{
	//Implementation with cpp std::unordered_map
	std::string key_str(index_ptr, len);
	htable_t *lock_table = (htable_t *)table;

	lock_table->insert(std::make_pair(key_str, (lock_object_t *)opaque_ptr));

	return true;
}

void *dfly_ht_find(void *table, char *index_ptr, uint32_t len)
{
	//Implementation with cpp std::unordered_map
	htable_t::const_iterator lock_obj_iter;
	htable_t *lock_table = (htable_t *)table;
	std::string key_str(index_ptr, len);

	lock_obj_iter =  lock_table->find(key_str);

	if (lock_obj_iter == lock_table->end()) {
		return NULL;
	} else {
		DFLY_ASSERT(lock_obj_iter->second);
		return lock_obj_iter->second;
	}

}

void dfly_ht_delete(void *table, char *index_ptr, uint32_t len)
{
	//Implementation with cpp std::unordered_map
	htable_t *lock_table = (htable_t *)table;
	std::string key_str(index_ptr, len);

	lock_table->erase(key_str);
}
/**
 * End Hash APIs
 */

static inline bool is_blocking_allowed(struct dfly_request *req)
{
	if (req->dqpair->npending_lock_reqs < req->dqpair->max_pending_lock_reqs) {
		return true;
	} else {
		return false;
	}
}

static inline bool dfly_nvmf_is_valid_lock_req(struct dfly_request *req)
{
	lock_request_t *cmd = (lock_request_t *)dfly_get_nvme_cmd(req);

	//Validate common fields
	if (cmd->opt.lock.priority != 0) {
		DFLY_WARNLOG("Invalid priority in lock request %p\n", cmd);
		dfly_set_status_code(req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_INVALID_FIELD);
		return false;
	}

	if (cmd->nsid == 0 || cmd->nsid > 1/*Only one nsid support*/) {
		DFLY_WARNLOG("Invalid key space id %d in lock request %p\n", cmd->nsid, cmd);
		dfly_set_status_code(req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_INVALID_FIELD);
		return false;
	}

	if (cmd->opt.lock.keylen > 15) {
		DFLY_WARNLOG("Invalid key length in lock request %p\n", cmd);
		dfly_set_status_code(req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_INVALID_FIELD);
		return false;
	}

	//Fields specific to request
	switch (cmd->opc) {
	case SPDK_NVME_OPC_SAMSUNG_KV_LOCK:
		if (cmd->opt.lock.blocking && \
		    (req->dqpair->max_pending_lock_reqs == 0)) {
			//Blocking lock command not supported for QD = 1
			dfly_set_status_code(req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_INVALID_FIELD);
			DFLY_WARNLOG("Blocking not supportted for queue depth 1 \n");
			return false;
		}
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_UNLOCK:
		if (cmd->opt.lock.blocking) {
			DFLY_WARNLOG("Ignoring blocking field for unlock command\n");
		}
		break;
	default:
		break;
	}

	return true;
}

static int verify_uuid(struct dfly_request *req, lock_object_t *lobj)
{
	bool uuid_op_rc = false;
	lock_request_t *lock_cmd = (lock_request_t *)dfly_get_nvme_cmd(req);

	//UUID check
	switch (lock_cmd->opc) {

	case SPDK_NVME_OPC_SAMSUNG_KV_LOCK:
		uuid_op_rc = dfly_u64ht_insert((void *)lobj->uuid_ht, lock_cmd->uuid);
		break;
	case SPDK_NVME_OPC_SAMSUNG_KV_UNLOCK:
		uuid_op_rc = dfly_u64ht_delete((void *)lobj->uuid_ht, lock_cmd->uuid);
		break;
	default:
		DFLY_ASSERT(false);
		break;

	}

	if (uuid_op_rc == false) {
		//UUID collision/Mismatch return Error
		DFLY_INFOLOG(DFLY_LOCK_SVC, "UUID error for key %llx:%llx uuid:%llx\n", lobj->key.key_p1,
			     lobj->key.key_p2, lock_cmd->uuid);
		dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_UUID_MISMATCH);
		dfly_nvmf_complete(req);
		return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
	}//End UUID check
	// Continue processing request otherwise
	return 0;
}

static inline int _dfly_nvmf_exec_lock(struct dfly_request *req, lock_object_t *lobj)
{
	lock_request_t *lock_cmd = (lock_request_t *)dfly_get_nvme_cmd(req);

	if (lock_cmd->opt.lock.write_lock) {
		//DFLY_ASSERT(lock_cmd->opt.lock.blocking == 0);
		//Write Lock
		if (!lobj->wlock && (lobj->rcount == 0)) {
			if (verify_uuid(req, lobj)) {
				return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
			}
			lobj->wlock = 1;
			//Success
			DFLY_INFOLOG(DFLY_LOCK_SVC, "Write Locked key %llx:%llx\n", lobj->key.key_p1, lobj->key.key_p2);
		} else {
			//Already locked
			if (lock_cmd->opt.lock.blocking && is_blocking_allowed(req)) {
				TAILQ_INSERT_TAIL(&lobj->pending_lock_reqs, req, lock_pending);
				req->dqpair->npending_lock_reqs++;
				return DFLY_MODULE_REQUEST_QUEUED;
			} else {
				dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_KEY_IS_LOCKED);
				DFLY_INFOLOG(DFLY_LOCK_SVC, "Write lock for locked key %llx:%llx\n", lobj->key.key_p1,
					     lobj->key.key_p2);
			}
		}
		dfly_nvmf_complete(req);
		return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
	} else {
		//Read Lock
		if (!lobj->wlock) {
			DFLY_ASSERT(lobj->rcount < lobj->rcount + 1);
			if (verify_uuid(req, lobj)) {
				return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
			}
			lobj->rcount++;
			//Update lock duration
			uint64_t new_expire_tick = spdk_get_ticks() + (lock_cmd->lock_duration * spdk_get_ticks_hz());
			if (new_expire_tick > lobj->expire_tick) {
				lobj->expire_tick = new_expire_tick;
				DFLY_INFOLOG(DFLY_LOCK_SVC, "Expire timer updated for key %llx:%llx\n exp_tick:%llx now_tick:%llx",
					     \
					     lobj->key.key_p1, lobj->key.key_p2, lobj->expire_tick, spdk_get_ticks());
				DFLY_ASSERT(lobj->expire_tick > spdk_get_ticks());
			}
			//Success
			DFLY_INFOLOG(DFLY_LOCK_SVC, "Read Locked key %llx:%llx\n", lobj->key.key_p1, lobj->key.key_p2);
		} else {
			if (lock_cmd->opt.lock.blocking  && is_blocking_allowed(req)) {
				TAILQ_INSERT_TAIL(&lobj->pending_lock_reqs, req, lock_pending);
				req->dqpair->npending_lock_reqs++;
				return DFLY_MODULE_REQUEST_QUEUED;
			} else {
				dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_KEY_IS_LOCKED);
				DFLY_INFOLOG(DFLY_LOCK_SVC, "Read lock for write locked key %llx:%llx\n", lobj->key.key_p1,
					     lobj->key.key_p2);
			}
		}
	}
	return DFLY_MODULE_REQUEST_PROCESSED;
}

//static inline int _dfly_nvmf_exec_unlock()

void dfly_process_pending_lock_reqs(lock_object_t *lobj)
{
	enum process_mode {
		REQ_PROCESSING_INIT = 0,
		REQ_PROCESSING_READERS = 1,
		REQ_PROCESSING_WRITER = 2,
	};

	int mode = REQ_PROCESSING_INIT;// 0:Starting,1:Readers,2:Writer
	int rc;

	lock_request_t *lock_cmd;

	struct dfly_request *pending_req, *req_tmp;

	TAILQ_FOREACH_SAFE(pending_req, &lobj->pending_lock_reqs, lock_pending, req_tmp) {
		lock_cmd = (lock_request_t *)dfly_get_nvme_cmd(pending_req);

		DFLY_ASSERT(lock_cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_LOCK);
		DFLY_ASSERT(lock_cmd->opt.lock.blocking);

		DFLY_ASSERT(pending_req->dqpair->npending_lock_reqs > 0);
		if (lock_cmd->opt.lock.write_lock) {
			if (mode == REQ_PROCESSING_INIT) {
				TAILQ_REMOVE(&lobj->pending_lock_reqs, pending_req, lock_pending);
				pending_req->dqpair->npending_lock_reqs--;
				rc = _dfly_nvmf_exec_lock(pending_req, lobj);
				DFLY_ASSERT(rc != DFLY_MODULE_REQUEST_QUEUED);
				if (rc == DFLY_MODULE_REQUEST_PROCESSED) {
					dfly_nvmf_complete(pending_req);
				}
				break;
			} else if (mode == REQ_PROCESSING_READERS) {
				break;
			}
		} else {
			mode = REQ_PROCESSING_READERS;
			TAILQ_REMOVE(&lobj->pending_lock_reqs, pending_req, lock_pending);
			pending_req->dqpair->npending_lock_reqs--;
			rc = _dfly_nvmf_exec_lock(pending_req, lobj);
			DFLY_ASSERT(rc != DFLY_MODULE_REQUEST_QUEUED);
			if (rc == DFLY_MODULE_REQUEST_PROCESSED) {
				dfly_nvmf_complete(pending_req);
			}
		}
	}

}

int dfly_nvmf_lock_svc_process_req(void *mctx, struct dfly_request *req)
{
	struct lock_service_subsys_s *ls_ss_ctx = (struct lock_service_subsys_s *)mctx;
	lock_request_t *lock_cmd = (lock_request_t *)dfly_get_nvme_cmd(req);

	lock_object_t *lobj = NULL;
	bool lobj_found = true;
	int rc = DFLY_MODULE_REQUEST_PROCESSED_INLINE;

	DFLY_INFOLOG(DFLY_LOCK_SVC, "LOCK::opc=%x, uuid=%llx, duration=%u," \
		     "len=%u, \npriority=%u, write_lock:%u, blocking:%u, key:\n", \
		     lock_cmd->opc, lock_cmd->uuid, lock_cmd->lock_duration, \
		     lock_cmd->opt.lock.keylen, lock_cmd->opt.lock.priority, \
		     lock_cmd->opt.lock.write_lock, lock_cmd->opt.lock.blocking);

	lock_object_t *ptr;

	if (dfly_nvmf_is_valid_lock_req(req)) {
		//Process Lock request;
		lobj =  (lock_object_t *)dfly_ht_find((void *)ls_ss_ctx->active_locks, (char *)&lock_cmd->key,
				     lock_cmd->opt.lock.keylen + 1);

		if (lock_cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_UNLOCK && !lobj) {
			//Unlock recieved without any lock present
			dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_KEY_NOT_EXIST);
			DFLY_INFOLOG(DFLY_LOCK_SVC, "Unlock for not-existing key %llx:%llx\n", lock_cmd->key.key_p1,
				     lock_cmd->key.key_p2);
			dfly_nvmf_complete(req);
			return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
		}

		if (!lobj) {
			lobj_found = false;
			lobj = (lock_object_t *)dfly_mempool_get(ls_ss_ctx->lobj_mp);
			if (!lobj) {
				//Return failure;
				DFLY_WARNLOG("Alloc for lock object failed\n");
				dfly_set_status_code(req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_INTERNAL_DEVICE_ERROR);
				dfly_nvmf_complete(req);
				return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
			}

			lobj->key = lock_cmd->key;
			lobj->klen = lock_cmd->opt.lock.keylen + 1;
			lobj->uuid_ht = new htable_u64_t;
			TAILQ_INIT(&lobj->pending_lock_reqs);

			//Setup expiry timer
			lobj->expire_tick  = spdk_get_ticks() + \
					     (lock_cmd->lock_duration * spdk_get_ticks_hz());
			DFLY_INFOLOG(DFLY_LOCK_SVC, "Expire timer set for key %llx:%llx\n exp_tick:%llx now_tick:%llx", \
				     lobj->key.key_p1, lobj->key.key_p2, lobj->expire_tick, spdk_get_ticks());
			DFLY_ASSERT(lobj->expire_tick > spdk_get_ticks());

			dfly_ht_insert((void *)ls_ss_ctx->active_locks, (char *)&lobj->key, lobj->klen, lobj);
		} else {
			if (lobj->expire_tick < spdk_get_ticks()) {
				//Lock Expired
				DFLY_WARNLOG("Locked expired for key %llx:%llx\n", lobj->key.key_p1, lobj->key.key_p2);
				//Cleanup
				struct dfly_request *pending_req, *req_tmp;
				TAILQ_FOREACH_SAFE(pending_req, &lobj->pending_lock_reqs, lock_pending, req_tmp) {
					TAILQ_REMOVE(&lobj->pending_lock_reqs, pending_req, lock_pending);
					req->dqpair->npending_lock_reqs--;
					dfly_set_status_code(pending_req, SPDK_NVME_SCT_KV_CMD, \
							     SPDK_NVME_SC_KV_LOCK_EXPIRED);
					dfly_nvmf_complete(pending_req);
				}
				DFLY_ASSERT(req->dqpair->npending_lock_reqs == 0);
				dfly_ht_delete((void *)ls_ss_ctx->active_locks, \
					       (char *)&lobj->key, lobj->klen);
				memset(lobj, 0, sizeof(*lobj));
				dfly_mempool_put(ls_ss_ctx->lobj_mp, lobj);
				// Return error
				dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, \
						     SPDK_NVME_SC_KV_LOCK_EXPIRED);
				dfly_nvmf_complete(req);
				return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
			}
		}
		if (lock_cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_LOCK) {
			rc = _dfly_nvmf_exec_lock(req, lobj);
			if (rc == DFLY_MODULE_REQUEST_PROCESSED_INLINE || \
			    rc == DFLY_MODULE_REQUEST_QUEUED) {
				return rc;
			}
		} else {//SPDK_NVME_OPC_SAMSUNG_KV_UNLOCK
			//Unlock cannot have newly created object
			DFLY_ASSERT(lobj_found == true);
			if (lock_cmd->opt.lock.write_lock) {
				//Unlock writer
				if (lobj->wlock) {
					if (verify_uuid(req, lobj)) {
						return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
					}
					DFLY_ASSERT(lobj->rcount == 0);
					lobj->wlock = 0;
					DFLY_INFOLOG(DFLY_LOCK_SVC, "Write UnLocked key %llx:%llx\n", lobj->key.key_p1, lobj->key.key_p2);
					dfly_process_pending_lock_reqs(lobj);
				} else {
					//No Writer exist
					DFLY_ASSERT(lobj->rcount > 0);
					dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_NO_WRITER_EXISTS);
					DFLY_INFOLOG(DFLY_LOCK_SVC, "Write Unlock non-existing writer key %llx:%llx\n", lobj->key.key_p1,
						     lobj->key.key_p2);
				}
			} else {
				//Unlock reader
				if (lobj->rcount > 0) {
					if (verify_uuid(req, lobj)) {
						return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
					}
					DFLY_ASSERT(lobj->wlock == 0);
					lobj->rcount--;
					DFLY_INFOLOG(DFLY_LOCK_SVC, "Read UnLocked key %llx:%llx\n", lobj->key.key_p1, lobj->key.key_p2);
					if (lobj->rcount == 0) {
						dfly_process_pending_lock_reqs(lobj);
					}
				} else {
					//No Reader exist
					DFLY_ASSERT(lobj->wlock);
					dfly_set_status_code(req, SPDK_NVME_SCT_KV_CMD, SPDK_NVME_SC_KV_NO_READER_EXISTS);
					DFLY_INFOLOG(DFLY_LOCK_SVC, "Read Unlock non-existing reader key %llx:%llx\n", lobj->key.key_p1,
						     lobj->key.key_p2);
				}
			}
			if (lobj->wlock == 0 && lobj->rcount == 0) {
				dfly_ht_delete((void *)ls_ss_ctx->active_locks, (char *)&lobj->key, lobj->klen);
				dfly_mempool_put(ls_ss_ctx->lobj_mp, lobj);
			}
		}

	}

	dfly_nvmf_complete(req);
	return DFLY_MODULE_REQUEST_PROCESSED_INLINE;
}

struct dfly_module_ops lock_svc_module_ops {
	.module_init_instance_context = NULL,
	.module_rpoll = dfly_nvmf_lock_svc_process_req,
	.module_cpoll = NULL,
	.module_gpoll = NULL,
	.find_instance_context = NULL,
	.module_instance_destroy = NULL,
};

int dfly_lock_service_subsys_start(struct dfly_subsystem *subsys, void *arg/*Not used*/,
				   df_module_event_complete_cb cb, void *cb_arg)
{
	int rc = 0;
	struct lock_service_subsys_s *ls_ss_ctx;

	ls_ss_ctx = (struct lock_service_subsys_s *)calloc(1, sizeof(struct lock_service_subsys_s));
	assert(ls_ss_ctx);
	if (ls_ss_ctx == NULL) {
		return -1;
	}

	ls_ss_ctx->id = subsys->id;

	ls_ss_ctx->active_locks = new htable_t;

	if (!ls_ss_ctx->active_locks) {
		free(ls_ss_ctx);
		return -1;
	}

	char lobj_mp_name[64];
	memset(lobj_mp_name, 0, 64);
	sprintf(lobj_mp_name, "lobj_mempool_%d", subsys->id);

	ls_ss_ctx->lobj_mp = dfly_mempool_create(lobj_mp_name, sizeof(lock_object_t), LOBJ_MPOOL_COUNT);
	if (!ls_ss_ctx->lobj_mp) {
		free(ls_ss_ctx);
		return -1;
	}

	subsys->mlist.lock_service = dfly_module_start("Lock_service", subsys->id, &lock_svc_module_ops,
				     ls_ss_ctx, 1, cb, cb_arg);

	DFLY_ASSERT(subsys->mlist.lock_service);

	return 0;
}

void _dfly_lock_service_subsystem_stop(void *event, void *ctx)
{
	struct df_ss_cb_event_s *lock_cb_event = (struct df_ss_cb_event_s *)event;
	struct lock_service_subsys_s *ls_ss_ctx = (struct lock_service_subsys_s *)ctx;

	dfly_mempool_destroy(ls_ss_ctx->lobj_mp, LOBJ_MPOOL_COUNT);
	ls_ss_ctx->active_locks = NULL;

	memset(ls_ss_ctx, 0, sizeof(*ls_ss_ctx));

	free(ls_ss_ctx);
	df_ss_cb_event_complete(lock_cb_event);

	return;
}
void dfly_lock_service_subsystem_stop(struct dfly_subsystem *subsys, void *arg/*Not used*/,
				   df_module_event_complete_cb cb, void *cb_arg)
{
	struct lock_service_subsys_s *ls_ss_ctx;
	struct df_ss_cb_event_s *lock_cb_event;

	ls_ss_ctx = (struct lock_service_subsys_s *)subsys->mlist.lock_service->ctx;
	lock_cb_event = df_ss_cb_event_allocate(subsys, cb, cb_arg, arg);

	DFLY_ASSERT(ls_ss_ctx);

	dfly_module_stop(subsys->mlist.lock_service, _dfly_lock_service_subsystem_stop, lock_cb_event, ls_ss_ctx);


	return;
}
