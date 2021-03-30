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


#include "dragonfly.h"

extern struct wal_conf_s g_wal_conf;
extern struct fuse_conf_s g_fuse_conf;
extern struct list_conf_s g_list_conf;

int dfly_handle_request(struct dfly_request *dfly_req)
{
	struct dfly_subsystem *ss       = dfly_get_subsystem_no_lock(dfly_req->req_ssid);
	uint32_t cmd_opc;
hop_2_next:
	switch (dfly_req->state) {
	case DFLY_REQ_BEGIN:
		//TODO: Initialization based on originator
		if (dfly_req->flags & DFLY_REQF_NVMF) {
			dfly_req_init_nvmf_value(dfly_req);
		} else if (dfly_req->flags & DFLY_REQF_NVME) {
			dfly_req_init_nvme_info(dfly_req);
		} else {
			assert(0);
		}

		//State Transition
		dfly_req->state = DFLY_REQ_INITIALIZED;
		goto hop_2_next;//Short circuit next state processing

		break;
	case DFLY_REQ_INITIALIZED:
		cmd_opc = dfly_req_get_command(dfly_req);
		if(spdk_unlikely(g_dragonfly->test_nic_bw &&
			(cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE ||
			 cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE))) {
			if(cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
				  dfly_resp_set_cdw0(dfly_req, dfly_req->req_value.length);
			}
			dfly_set_status_code(dfly_req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_SUCCESS);
			dfly_nvmf_complete(dfly_req);
			break;
		}
		if(spdk_unlikely(g_dragonfly->test_sim_io_timeout &&
			(cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE ||
			 cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE))) {
				usleep(g_dragonfly->test_sim_io_timeout * 1000000);
		}
		if (cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_LOCK
		    || cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_UNLOCK) {
			if (ss->mlist.lock_service) { //Lock service supported
				//Forward to lock service module
				dfly_module_post_request(ss->mlist.lock_service, dfly_req);
			} else {
				//Return unsupported commmand
				dfly_set_status_code(dfly_req, SPDK_NVME_SCT_GENERIC, SPDK_NVME_SC_INVALID_OPCODE);
				dfly_nvmf_complete(dfly_req);
			}
			break;
		} else if (SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ == dfly_req->ops.get_command(dfly_req)) {
			dfly_req->state = DFLY_REQ_IO_NVMF_DONE;
			goto hop_2_next;
		} else if (g_fuse_conf.fuse_enabled
			   && !(dfly_req->flags & DFLY_REQF_NVME)/*Flush request*/
			   && dfly_req->ops.get_command(dfly_req) != SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE) {
			//State Transition
			dfly_req->state = DFLY_REQ_IO_SUBMITTED_TO_FUSE;
			dfly_module_post_request(ss->mlist.dfly_fuse_module, dfly_req);
		} else if ((g_wal_conf.wal_cache_enabled || g_wal_conf.wal_log_enabled)
			   && !(dfly_req->flags & DFLY_REQF_NVME)/*Flush request*/) {
			//State Transition
			dfly_req->state = DFLY_REQ_IO_SUBMITTED_TO_WAL;
			dfly_module_post_request(ss->mlist.dfly_wal_module, dfly_req);
		} else {//Forward to IO Threads
			//State Transition
			dfly_req->state = DFLY_REQ_IO_SUBMITTED_TO_DEVICE;
			dfly_module_post_request(ss->mlist.dfly_io_module, dfly_req);
		}
		break;
	case DFLY_REQ_IO_SUBMITTED_TO_FUSE:
		if (dfly_req->next_action == DFLY_FORWARD_TO_WAL) {
			dfly_req->state = DFLY_REQ_IO_SUBMITTED_TO_WAL;
			dfly_module_post_request(ss->mlist.dfly_wal_module, dfly_req);
		} else if (dfly_req->next_action == DFLY_FORWARD_TO_IO_THREAD) {
			dfly_req->state = DFLY_REQ_IO_SUBMITTED_TO_DEVICE;
			dfly_module_post_request(ss->mlist.dfly_io_module, dfly_req);
		} else if (dfly_req->next_action == DFLY_COMPLETE_NVMF) {
			//dfly_nvmf_complete(dfly_req);
			dfly_req->state = DFLY_REQ_IO_NVMF_DONE;
			goto hop_2_next;
			//printf(" dfly_nvmf_complete fuse %p\n", dfly_req);
		} else {
			assert(0);
		}
		break;
	case DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL:
		if (dfly_req->parent) {
			printf("req->flags %x parent_flags %x\n", dfly_req->flags, dfly_req->parent->flags);
#if 0
			dfly_module_complete_request(ss->mlist.dfly_fuse_module,
						     dfly_req->parent);
			dfly_io_put_req(NULL, dfly_req);
#else
			dfly_module_complete_request(ss->mlist.dfly_fuse_module, dfly_req);
#endif
		} else {
			assert(0);
		}
		break;
	case DFLY_REQ_IO_SUBMITTED_TO_WAL:
		if (dfly_req->next_action == DFLY_COMPLETE_NVMF) {
			if (dfly_req->req_fuse_data) {
				dfly_module_complete_request(ss->mlist.dfly_fuse_module, dfly_req);
			} else {
				//dfly_nvmf_complete(dfly_req);
				dfly_req->state = DFLY_REQ_IO_NVMF_DONE;
				goto hop_2_next;
			}
		} else if (dfly_req->next_action == DFLY_FORWARD_TO_IO_THREAD) {
			dfly_req->state = DFLY_REQ_IO_SUBMITTED_TO_DEVICE;
			dfly_module_post_request(ss->mlist.dfly_io_module, dfly_req);
		} else {
			assert(0);
		}
		break;
	case DFLY_REQ_IO_SUBMITTED_TO_DEVICE:
		if (dfly_req->next_action == DFLY_COMPLETE_ON_FUSE_THREAD) {
			dfly_req->state = DFLY_REQ_IO_COMPLETED_FROM_DEVICE;
			dfly_module_complete_request(ss->mlist.dfly_fuse_module, dfly_req);
		} else if (dfly_req->next_action == DFLY_COMPLETE_NVMF) {
			if (dfly_req->parent && dfly_req->parent->req_fuse_data) {
				dfly_module_complete_request(ss->mlist.dfly_fuse_module, dfly_req->parent);
				printf(" dfly_handle_request req->parent %p, compete %p\n",
				       dfly_req->parent, dfly_req);
			}
			//printf("dfly_handle_request req %p state DFLY_REQ_IO_SUBMITTED_TO_DEVICE\n", dfly_req);
			// dfly_req->state = DFLY_REQ_IO_COMPLETED_FROM_DEVICE;
			//dfly_nvmf_complete(dfly_req);
			dfly_req->state = DFLY_REQ_IO_NVMF_DONE;
			goto hop_2_next;
		} else {
			assert(0);
		}
		break;
	case DFLY_REQ_IO_COMPLETED_FROM_DEVICE:
		assert(dfly_req->next_action == DFLY_COMPLETE_IO);
		if (ss->mlist.dfly_fuse_module) {
			dfly_module_complete_request(ss->mlist.dfly_fuse_module, dfly_req);
		} else {
			//printf("dfly_handle_request req %p state DFLY_REQ_IO_COMPLETED_FROM_DEVICE\n", dfly_req);
			//dfly_nvmf_complete(dfly_req);
			dfly_req->state = DFLY_REQ_IO_NVMF_DONE;
			goto hop_2_next;
		}
		break;
	case DFLY_REQ_IO_NVMF_DONE:
		cmd_opc = dfly_req->ops.get_command(dfly_req);
		if (!(cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE
		      || cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_DELETE
		      || cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_OPEN
		      || cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_CLOSE
		      || cmd_opc == SPDK_NVME_OPC_SAMSUNG_KV_LIST_READ))
			dfly_nvmf_complete(dfly_req);
		else if ((dfly_req->flags & DFLY_REQF_NVMF && g_list_conf.list_enabled)
			 && (dfly_req->next_action != DFLY_REQ_IO_LIST_DONE)) {
			dfly_req->state = DFLY_REQ_IO_LIST_FORWARD;
			dfly_module_post_request(ss->mlist.dfly_list_module, dfly_req);
			//list_io(dfly_req);
		} else {
			dfly_nvmf_complete(dfly_req);
		}
		break;

	default:
		assert(0);//Unhandled state
		break;
	}

}
