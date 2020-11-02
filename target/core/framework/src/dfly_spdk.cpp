#include "dragonfly.h"
#include "nvmf_internal.h"

#include "rocksdb/dss_kv2blk_c.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sched.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

struct spdk_nvme_cmd *dfly_nvmf_setup_cmd_key(struct spdk_nvmf_request *req,
		struct spdk_nvme_cmd *in_cmd);

static int
dfly_spdk_get_numa_node_value(char *path)
{
	FILE *fd;
	int numa_node = -1;
	char buf[MAX_STRING_LEN + 1];

	fd = fopen(path, "r");
	if (!fd) {
		return -1;
	}

	if (fgets(buf, sizeof(buf), fd) != NULL) {
		numa_node = strtoul(buf, NULL, 10);
	}
	fclose(fd);

	return numa_node;
}

int _dfly_spdk_get_ifaddr_numa_node(char *if_addr)
{
	int ret;
	struct ifaddrs *ifaddrs, *ifa;
	struct sockaddr_in addr, addr_in;
	char path[MAX_STRING_LEN + 1];
	int numa_node = -1;

	addr_in.sin_addr.s_addr = inet_addr(if_addr);
	if (addr_in.sin_addr.s_addr == INADDR_NONE) {
		return -1;//We don't expect 255.255.255.255
		//Should be a local NIC
	}

	ret = getifaddrs(&ifaddrs);
	if (ret < 0)
		return -1;

	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		addr = *(struct sockaddr_in *)ifa->ifa_addr;
		if ((uint32_t)addr_in.sin_addr.s_addr != (uint32_t)addr.sin_addr.s_addr) {
			continue;
		}
		snprintf(path, MAX_STRING_LEN, "/sys/class/net/%s/device/numa_node", ifa->ifa_name);
		numa_node = dfly_spdk_get_numa_node_value(path);
		break;
	}
	freeifaddrs(ifaddrs);

	return numa_node;
}

void
dfly_nvmf_bdev_ctrlr_complete_cmd(struct spdk_bdev_io *bdev_io, bool success,
				  void *cb_arg)
{
	struct spdk_nvmf_request	*req = (struct spdk_nvmf_request *)cb_arg;
	struct spdk_nvme_cpl		*response = &req->rsp->nvme_cpl;
	int				sc, sct;
	uint32_t        cdw0;

	struct dfly_request *dreq;

	spdk_bdev_io_get_nvme_status(bdev_io, &cdw0, &sct, &sc);
	response->status.sc = sc;
	response->status.sct = sct;
	response->cdw0 = cdw0;


	dreq = req->dreq;
	assert(dreq != NULL);

	assert(dreq->state == DFLY_REQ_IO_SUBMITTED_TO_DEVICE);

	dreq->status = success;

	dreq->state = DFLY_REQ_IO_COMPLETED_FROM_DEVICE;
	dreq->next_action = DFLY_COMPLETE_IO;

	spdk_bdev_free_io(bdev_io);
	dfly_handle_request(dreq);

	return;
}

int
_dfly_nvmf_ctrlr_process_io_cmd(struct io_thread_inst_ctx_s *thrd_inst,
				struct spdk_nvmf_request *req)
{
	uint32_t nsid;
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *desc;
	struct spdk_io_channel *ch;
	struct spdk_nvmf_poll_group *group = req->qpair->group;
	struct spdk_nvmf_ctrlr *ctrlr = req->qpair->ctrlr;
	struct spdk_nvme_cmd *cmd = NULL;
	struct spdk_nvme_cmd tmp_cmd;
	struct spdk_nvme_cpl *response = &req->rsp->nvme_cpl;
	struct spdk_nvmf_subsystem  *subsys = ctrlr->subsys;
	struct dfly_io_device_s *io_device;
	int rc;

	cmd = dfly_nvmf_setup_cmd_key(req, &tmp_cmd);
	assert(cmd);

	/* pre-set response details for this command */
	response->status.sc = SPDK_NVME_SC_SUCCESS;
	nsid = cmd->nsid;

	if (spdk_unlikely(ctrlr == NULL)) {
		SPDK_ERRLOG("I/O command sent before CONNECT\n");
		response->status.sct = SPDK_NVME_SCT_GENERIC;
		response->status.sc = SPDK_NVME_SC_COMMAND_SEQUENCE_ERROR;
		return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
	}

	if (spdk_unlikely(ctrlr->vcprop.cc.bits.en != 1)) {
		SPDK_ERRLOG("I/O command sent to disabled controller\n");
		response->status.sct = SPDK_NVME_SCT_GENERIC;
		response->status.sc = SPDK_NVME_SC_COMMAND_SEQUENCE_ERROR;
		return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
	}

	io_device = (struct dfly_io_device_s *) req->dreq->io_device;
	DFLY_ASSERT(io_device);

	bdev = io_device->ns->bdev;
	desc = io_device->ns->desc;
	ch   = thrd_inst->io_chann_parr[io_device->index];

	dfly_counters_increment_io_count(io_device->stat_io, cmd->opc);
	if (cmd->opc == SPDK_NVME_OPC_SAMSUNG_KV_STORE) {
		dfly_counters_size_count(io_device->stat_io, req, cmd->opc);
		dfly_counters_bandwidth_cal(io_device->stat_io, req, cmd->opc);
	}

	//printf("_dfly_nvmf_ctrlr_process_io_cmd opc %d icore %d\n", cmd->opc, sched_getcpu());
	if(df_subsystem_enabled(subsys->id) &&
		g_dragonfly->blk_map) {//Rocksdb block trannslation
		switch (cmd->opc) {
			case SPDK_NVME_OPC_SAMSUNG_KV_STORE:
				//SPDK_NOTICELOG("Rocksdb put started\n");
				rc = dss_rocksdb_put(io_device->rdb_handle->rdb_db_handle,
										req->dreq->req_key.key, req->dreq->req_key.length,
										req->dreq->req_value.value, req->dreq->req_value.length);
				if(rc == -1) {
					SPDK_ERRLOG("Rocksdb put failed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
				} else {
					DFLY_ASSERT(rc == 0);
					//SPDK_NOTICELOG("Rocksdb put completed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_SUCCESS;
				}
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
				break;
			case SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE:
				//SPDK_NOTICELOG("Rocksdb get started\n");
				dss_rocksdb_get_async(thrd_inst, io_device->rdb_handle->rdb_db_handle,
										req->dreq->req_key.key, req->dreq->req_key.length,
										req->dreq->req_value.value, req->dreq->req_value.length,
										NULL, req);
				return SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS;
				break;
			case SPDK_NVME_OPC_SAMSUNG_KV_DELETE:
				rc = dss_rocksdb_delete(io_device->rdb_handle->rdb_db_handle,
										req->dreq->req_key.key,
										req->dreq->req_key.length);
				if(rc == -1) {
					SPDK_ERRLOG("Rocksdb delete failed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
				} else {
					DFLY_ASSERT(rc == 0);
					//SPDK_NOTICELOG("Rocksdb delete completed\n");
					req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
					req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_SUCCESS;
				}
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
				break;
			default:
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
				return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
				break;
		}
		DFLY_ASSERT(0);
	}
	switch (cmd->opc) {
	case SPDK_NVME_OPC_READ:
	case SPDK_NVME_OPC_WRITE:
	case SPDK_NVME_OPC_WRITE_ZEROES:
	case SPDK_NVME_OPC_FLUSH:
	case SPDK_NVME_OPC_DATASET_MANAGEMENT:
	//assert(0);//Non Key value command
	//break;
	default:
		rc = spdk_bdev_nvme_io_passthru(desc, ch, cmd, req->data, req->length,
						dfly_nvmf_bdev_ctrlr_complete_cmd, req);
		if (spdk_unlikely(rc)) {
			if (rc == -ENOMEM) {
				DFLY_ERRLOG("submit io failed with no mem\n");
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
			} else {
				req->rsp->nvme_cpl.status.sct = SPDK_NVME_SCT_GENERIC;
				req->rsp->nvme_cpl.status.sc = SPDK_NVME_SC_INVALID_OPCODE;
			}
			return SPDK_NVMF_REQUEST_EXEC_STATUS_COMPLETE;
		}
	}

	return SPDK_NVMF_REQUEST_EXEC_STATUS_ASYNCHRONOUS;
}
