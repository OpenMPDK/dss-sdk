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


#include "nvme_internal.h"
#include "spdk/nvme_samsung_spec.h"
#include "spdk/nvme_samsung_apis.h"

struct dword_bytes {
	uint8_t cdwb1;
	uint8_t cdwb2;
	uint8_t cdwb3;
	uint8_t cdwb4;
};

/*
 * Setup store request
 */
static void
_nvme_kv_cmd_setup_store_request(struct spdk_nvme_ns *ns, struct nvme_request *req,
				 uint32_t key_size,
				 uint32_t buffer_size,
				 uint32_t offset,
				 uint32_t io_flags, uint32_t option)
{
	struct spdk_nvme_cmd	*cmd;
	//uint32_t        lba = 0, lba_cnt = 0;
	struct dword_bytes *cdw11;

	cmd = &req->cmd;

	cdw11 =(struct dword_bytes *)&cmd->cdw11;

	if (option & 0x100) { //Fuse 1op
		assert(0);//Store cannot be first fuse OP
	} else if (option & 0x200) { //Fuse 2 op
		cmd->fuse = 0x2;
	}
	option &= 0xFF;

	//printf("setup_store_request fuse %d\n", cmd->fuse);

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = (buffer_size >> 2);
	cmd->nsid = ns->id;

	// cdw5:
	//    [2:31] The key_size(offset) of value  in bytes
	//    [0:1] Option
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (uint64_t)(offset << 2);
}

/*
 * Setup compare request
 */
static void
_nvme_kv_cmd_setup_cmp_request(struct spdk_nvme_ns *ns, struct nvme_request *req,
			       uint32_t key_size,
			       uint32_t buffer_size,
			       uint32_t offset,
			       uint32_t io_flags, uint32_t option)
{
	struct spdk_nvme_cmd	*cmd;
	//uint32_t        lba = 0, lba_cnt = 0;
	struct dword_bytes *cdw11;

	cmd = &req->cmd;

	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	if (option & 0x100) { //Fuse 1op
		cmd->fuse = 0x1;
	} else if (option & 0x200) { //Fuse 2 op
		assert(0);//Only fused 1 op
	}
	option &= 0xFF;

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = (buffer_size >> 2);
	cmd->nsid = ns->id;

	// cdw5:
	//    [2:31] The key_size(offset) of value  in bytes
	//    [0:1] Option
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (uint64_t)(offset << 2);
}

/*
 * Setup retrieve request
 */
static void
_nvme_kv_cmd_setup_retrieve_request(struct spdk_nvme_ns *ns, struct nvme_request *req,
				    uint32_t key_size, uint32_t buffer_size,
				    uint32_t offset,
				    uint32_t io_flags, uint32_t option)
{
	struct spdk_nvme_cmd    *cmd;
	//uint32_t        lba = 0, lba_cnt = 0;
	struct dword_bytes *cdw11;

	cmd = &req->cmd;

	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = (buffer_size >> 2);
	cmd->nsid = ns->id;

	// cdw5:
	//    [2:31] The offset of value  in bytes
	//    [0:1] Option
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (((uint64_t)offset) << 32);

}

/*
 */
static void
_nvme_kv_cmd_setup_delete_request(struct spdk_nvme_ns *ns, struct nvme_request *req,
				  uint32_t key_size,
				  uint32_t io_flags, uint32_t option)
{
	struct spdk_nvme_cmd    *cmd;
	struct dword_bytes *cdw11;

	cmd = &req->cmd;
	cmd->nsid = ns->id;

	if (option & 0x100) { //Fuse 1op
		assert(0);
	} else if (option & 0x200) { //Fuse 2 op
		cmd->fuse = 0x2;
	}
	option &= 0xFF;

	cdw11 = (struct dword_bytes *)&cmd->cdw11;

	cdw11->cdwb1 = key_size - 1;
	cdw11->cdwb2 = option;

	cmd->cdw10 = 0;

}


/*
 */
static void
_nvme_kv_cmd_setup_exist_request(struct spdk_nvme_ns *ns, struct nvme_request *req,
				 uint32_t key_size,
				 uint32_t key_number,
				 uint32_t buffer_size,
				 uint32_t io_flags, uint32_t option)
{
	struct spdk_nvme_cmd    *cmd;

	cmd = &req->cmd;

	cmd->cdw13 = key_number - 1;
	cmd->cdw14 = key_size - 1;
	cmd->cdw15 = buffer_size;
	cmd->nsid = ns->id;

	printf("Unsupported nvme-kvblock-cmd exist\n");

	// cdw5:
	//   Key length option ( 0 : fixed size, 1: variable size)
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (((uint64_t)option) << 32);
}

/*
 */
static void
_nvme_kv_cmd_setup_iterate_request(struct spdk_nvme_ns *ns, struct nvme_request *req,
				   uint8_t  *bitmask,
				   uint8_t  *iterator,
				   uint32_t buffer_size,
				   uint32_t io_flags, uint32_t option)
{
	struct spdk_nvme_cmd    *cmd;

	cmd = &req->cmd;

	cmd->cdw14 = (bitmask[0] << 24) | (bitmask[1] << 16) | (bitmask[2] << 8) | bitmask[3];
	cmd->cdw15 = buffer_size;
	// Bugbug:
	//   In the spec, iterator (3 bytes) should be set in CDW10-11 (8 bytes)
	//   Here, we use lower 3byter in CDW10 for iterator, please change it if
	//   it is not the case.
	cmd->cdw10 = (iterator[0] << 16) | (iterator[1] << 8) | iterator[2];
	cmd->nsid = ns->id;

	printf("Unsupported nvme-kvblock-cmd iterate\n");

	// cdw5:
	//   Key length option ( 0 : fixed size, 1: variable size)
	// MPTR: CDW4-5
	//    To minimize the modification to original SPDK code, still use mptr here
	cmd->mptr = (((uint64_t)option) << 32);
}

/*
 * _nvme_kv_cmd_allocate_request
 *   Allocate request and fill payload/metadata.
 *   We use metadata/payload in different ways in different commands.
 *                         metadata         payload
 *   Store/Retrieve        key              value
 *   Delete                key              N/A
 *   Exist                 result list      key array
 *   Iterate               N/A              key list
 */
static struct nvme_request *
_nvme_kv_cmd_allocate_request(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			      const struct nvme_payload *payload,
			      uint32_t buffer_size, uint32_t payload_offset, uint32_t md_offset,
			      spdk_nvme_cmd_cb cb_fn, void *cb_arg, uint32_t opc, uint32_t io_flags)
{
	struct nvme_request	*req;
	struct spdk_nvme_cmd    *cmd;

	if (io_flags & 0xFFFF) {
		/* We dont support any ioflag so far */
		return NULL;
	}

	req = nvme_allocate_request(qpair, payload, buffer_size, cb_fn, cb_arg);
	if (req == NULL) {
		return NULL;
	}

	cmd = &req->cmd;
	cmd->opc = opc;
	cmd->nsid = ns->id;

	req->payload_offset = payload_offset;
	req->md_offset = md_offset;

	return req;
}

#define PAGE_SZ_MASK (~(PAGE_SIZE -1))

void nvme_kv_cmd_setup_key(struct spdk_nvme_cmd *cmd, void *src_key, uint32_t keylen, void *dst_key)
{

	uint64_t phys_addr;

	assert(keylen <= SAMSUNG_KV_MAX_KEY_SIZE);

	if (keylen > SAMSUNG_KV_MAX_EMBED_KEY_SIZE) {
		//prp or sgl
		uint64_t *prp1, *prp2;
		char     *key_data;

		prp1 = (uint64_t *)&cmd->cdw12;
		prp2 = (uint64_t *)&cmd->cdw14;

		key_data = (char *)dst_key;

		key_data[255] = '\0';
		if (src_key) {
			memcpy(key_data, src_key, keylen);
		}

		phys_addr = spdk_vtophys(key_data, NULL);
		if (phys_addr == SPDK_VTOPHYS_ERROR) {
			SPDK_ERRLOG("vtophys(%p) failed\n", key_data);
			assert(0);
		}

		*prp1 = phys_addr;

		if (((uint64_t)(phys_addr + keylen - 1)  & PAGE_SZ_MASK) !=
		    (((uint64_t)phys_addr & PAGE_SZ_MASK))) {
			*prp2 = (uint64_t)((uint64_t)(phys_addr + keylen - 1) & PAGE_SZ_MASK);
			//SPDK_WARNLOG("key split across two prp PRP1:%p PRP2:%p \n", *prp1, *prp2);
			//assert(0);
		} else {
			*prp2 = 0;
		}
	} else {
		if (src_key) {
			memcpy(&cmd->cdw12, src_key, keylen);
		}
	}
}

/**
 * \brief Submits a KV Store I/O to the specified NVMe namespace.
 *
 * \param ns NVMe namespace to submit the KV Store I/O
 * \param qpair I/O queue pair to submit the request
 * \param key virtual address pointer to the value
 * \param key_length length (in bytes) of the key
 * \param buffer virtual address pointer to the value
 * \param buffer_length length (in bytes) of the value
 * \param offset offset of value (in bytes)
 * \param cb_fn callback function to invoke when the I/O is completed
 * \param cb_arg argument to pass to the callback function
 * \param io_flags set flags, defined by the SPDK_NVME_IO_FLAGS_* entries
 *                      in spdk/nvme_spec.h, for this I/O.
 * \param option option to pass to NVMe command
 *          0 - Idempotent; 1 - Post; 2 - Append
 * \return 0 if successfully submitted, ENOMEM if an nvme_request
 *           structure cannot be allocated for the I/O request, EINVAL if
 *           key_length or buffer_length is too large.
 *
 * The command is submitted to a qpair allocated by spdk_nvme_ctrlr_alloc_io_qpair().
 * The user must ensure that only one thread submits I/O on a given qpair at any given time.
 */
int
spdk_nvme_kv_cmd_store(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		       void *key, uint32_t key_length,
		       void *buffer, uint32_t buffer_length,
		       uint32_t offset,
		       spdk_nvme_cmd_cb cb_fn, void *cb_arg,
		       uint32_t io_flags,
		       uint32_t  option)
{
	struct nvme_request *req;
	struct nvme_payload payload;

	if (key_length > SAMSUNG_KV_MAX_KEY_SIZE) return -EINVAL;
	if (buffer_length > SAMSUNG_KV_MAX_VALUE_SIZE) return -EINVAL;
	if (!key_length || !buffer_length) return -EINVAL;

	//payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	//payload.u.contig = buffer;

	payload.reset_sgl_fn = NULL;
	payload.contig_or_cb_arg = buffer;

	//printf("spdk_nvme_kv_cmd_store 0x%llx%llx\n",
	//	*(long long *)key, *(long long *)(key+8));

	assert(buffer);

	req = _nvme_kv_cmd_allocate_request(ns, qpair, &payload, buffer_length,
					    offset, 0, cb_fn, cb_arg, SPDK_NVME_OPC_SAMSUNG_KV_STORE,
					    io_flags);

	if (NULL == req) {
		return -ENOMEM;
	}

	req->payload.md = NULL;
	nvme_kv_cmd_setup_key(&req->cmd, key, key_length, req->key_data);

	_nvme_kv_cmd_setup_store_request(ns, req,
					 key_length, buffer_length,
					 offset, io_flags, option);

	return nvme_qpair_submit_request(qpair, req);
}

/**
 * \brief Submits a KV Compare I/O to the specified NVMe namespace.
 *
 * \param ns NVMe namespace to submit the KV Compare I/O
 * \param qpair I/O queue pair to submit the request
 * \param key virtual address pointer to the value
 * \param key_length length (in bytes) of the key
 * \param buffer virtual address pointer to the value
 * \param buffer_length length (in bytes) of the value
 * \param offset offset of value (in bytes)
 * \param cb_fn callback function to invoke when the I/O is completed
 * \param cb_arg argument to pass to the callback function
 * \param io_flags set flags, defined by the SPDK_NVME_IO_FLAGS_* entries
 *                      in spdk/nvme_spec.h, for this I/O.
 * \param option option to pass to NVMe command
 *          0x100 Fused OP1 ;
 * \return 0 if successfully submitted, ENOMEM if an nvme_request
 *           structure cannot be allocated for the I/O request, EINVAL if
 *           key_length or buffer_length is too large.
 *
 * The command is submitted to a qpair allocated by spdk_nvme_ctrlr_alloc_io_qpair().
 * The user must ensure that only one thread submits I/O on a given qpair at any given time.
 */
int
spdk_nvme_kv_cmd_compare(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			 void *key, uint32_t key_length,
			 void *buffer, uint32_t buffer_length,
			 uint32_t offset,
			 spdk_nvme_cmd_cb cb_fn, void *cb_arg,
			 uint32_t io_flags,
			 uint32_t  option)
{
	struct nvme_request *req;
	struct nvme_payload payload;

	if (key_length > SAMSUNG_KV_MAX_KEY_SIZE) return -EINVAL;
	if (buffer_length > SAMSUNG_KV_MAX_VALUE_SIZE) return -EINVAL;
	if (!key_length || !buffer_length) return -EINVAL;

	//payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	//payload.u.contig = buffer;

	payload.reset_sgl_fn = NULL;
	payload.contig_or_cb_arg = buffer;

	req = _nvme_kv_cmd_allocate_request(ns, qpair, &payload, buffer_length,
					    offset, 0, cb_fn, cb_arg, SPDK_NVME_OPC_SAMSUNG_KV_CMP,
					    io_flags);

	if (NULL == req) {
		return -ENOMEM;
	}

	req->payload.md = NULL;
	nvme_kv_cmd_setup_key(&req->cmd, key, key_length, req->key_data);

	_nvme_kv_cmd_setup_cmp_request(ns, req,
				       key_length, buffer_length,
				       offset, io_flags, option);


	return nvme_qpair_submit_request(qpair, req);
}

/**
 * \brief Submits a KV Retrieve I/O to the specified NVMe namespace.
 *
 * \param ns NVMe namespace to submit the KV Retrieve I/O
 * \param qpair I/O queue pair to submit the request
 * \param key virtual address pointer to the value
 * \param key_length length (in bytes) of the key
 * \param buffer virtual address pointer to the value
 * \param buffer_length length (in bytes) of the value
 * \param offset offset of value (in bytes)
 * \param cb_fn callback function to invoke when the I/O is completed
 * \param cb_arg argument to pass to the callback function
 * \param io_flags set flags, defined by the SPDK_NVME_IO_FLAGS_* entries
 *                      in spdk/nvme_spec.h, for this I/O.
 * \param option option to pass to NVMe command
 *     No option supported for retrieve I/O yet.
 *
 * \return 0 if successfully submitted, ENOMEM if an nvme_request
 *           structure cannot be allocated for the I/O request, EINVAL if
 *           key_length or buffer_length is too large.
 *
 * The command is submitted to a qpair allocated by spdk_nvme_ctrlr_alloc_io_qpair().
 * The user must ensure that only one thread submits I/O on a given qpair at any given time.
 */
int
spdk_nvme_kv_cmd_retrieve(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			  void *key, uint32_t key_length,
			  void *buffer, uint32_t buffer_length,
			  uint32_t offset,
			  spdk_nvme_cmd_cb cb_fn, void *cb_arg,
			  uint32_t io_flags, uint32_t option)
{
	struct nvme_request *req;
	struct nvme_payload payload;

	if (key_length > SAMSUNG_KV_MAX_KEY_SIZE) return -EINVAL;
	if (!key_length || !buffer_length) return -EINVAL;

	//payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	//payload.u.contig = buffer;

	payload.reset_sgl_fn = NULL;
	payload.contig_or_cb_arg = buffer;

	req = _nvme_kv_cmd_allocate_request(ns, qpair, &payload, buffer_length,
					    offset, 0, cb_fn, cb_arg, SPDK_NVME_OPC_SAMSUNG_KV_RETRIEVE,
					    io_flags);
	if (NULL == req) {
		return -ENOMEM;
	}

	req->payload.md = NULL;
	nvme_kv_cmd_setup_key(&req->cmd, key, key_length, req->key_data);

	_nvme_kv_cmd_setup_retrieve_request(ns, req,
					    key_length, buffer_length, offset,
					    io_flags, option);

	return nvme_qpair_submit_request(qpair, req);
}

/**
 * \brief Submits a KV Delete I/O to the specified NVMe namespace.
 *
 * \param ns NVMe namespace to submit the KV DeleteI/O
 * \param qpair I/O queue pair to submit the request
 * \param key virtual address pointer to the value
 * \param key_length length (in bytes) of the key
 * \param cb_fn callback function to invoke when the I/O is completed
 * \param cb_arg argument to pass to the callback function
 * \param io_flags set flags, defined by the SPDK_NVME_IO_FLAGS_* entries
 *                      in spdk/nvme_spec.h, for this I/O.
 * \param option option to pass to NVMe command
 *     No option supported for retrieve I/O yet.
 *
 * \return 0 if successfully submitted, ENOMEM if an nvme_request
 *           structure cannot be allocated for the I/O request, EINVAL if
 *           key_length or buffer_length is too large.
 *
 * The command is submitted to a qpair allocated by spdk_nvme_ctrlr_alloc_io_qpair().
 * The user must ensure that only one thread submits I/O on a given qpair at any given time.
 */
int
spdk_nvme_kv_cmd_delete(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			void *key, uint32_t key_length,
			spdk_nvme_cmd_cb cb_fn, void *cb_arg,
			uint32_t io_flags, uint32_t  option)
{
	struct nvme_request *req;
	struct nvme_payload payload;

	if (key_length > SAMSUNG_KV_MAX_KEY_SIZE) return -EINVAL;

	//payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	//payload.u.contig = NULL;

	payload.reset_sgl_fn = NULL;
	payload.contig_or_cb_arg = NULL;

	//printf("spdk_nvme_kv_cmd_delete 0x%llx%llx\n",
	//		*(long long *)key, *(long long *)(key+8));

	req = _nvme_kv_cmd_allocate_request(ns, qpair, &payload,
					    0, //Payload length is 0 for delete command
					    0, 0, cb_fn, cb_arg, SPDK_NVME_OPC_SAMSUNG_KV_DELETE,
					    io_flags);

	if (NULL == req) {
		return -ENOMEM;
	}

	req->payload.md = NULL;
	nvme_kv_cmd_setup_key(&req->cmd, key, key_length, req->key_data);

	_nvme_kv_cmd_setup_delete_request(ns, req,
					  key_length, io_flags, option);

	return nvme_qpair_submit_request(qpair, req);

}


/**
 * \brief Submits a KV Exist I/O to the specified NVMe namespace.
 *
 * \param ns NVMe namespace to submit the KV Exist I/O
 * \param qpair I/O queue pair to submit the request
 * \param keys virtual address pointer to the key array
 * \param key_length length (in bytes) of the key
 * \param buffer virtual address pointer to the return buffer
 * \param buffer_length length (in bytes) of the return buffer
 * \param cb_fn callback function to invoke when the I/O is completed
 * \param cb_arg argument to pass to the callback function
 * \param io_flags set flags, defined by the SPDK_NVME_IO_FLAGS_* entries
 *                      in spdk/nvme_spec.h, for this I/O.
 * \param option option to pass to NVMe command
 *       0 - Fixed size; 1 - Variable size
 *
 * \return 0 if successfully submitted, ENOMEM if an nvme_request
 *           structure cannot be allocated for the I/O request, EINVAL if
 *           key_length or buffer_length is too large.
 *
 * The command is submitted to a qpair allocated by spdk_nvme_ctrlr_alloc_io_qpair().
 * The user must ensure that only one thread submits I/O on a given qpair at any given time.
 */
int
spdk_nvme_kv_cmd_exist(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
		       void *keys, uint32_t key_number, uint32_t key_length,
		       void *buffer, uint32_t buffer_length,
		       spdk_nvme_cmd_cb cb_fn, void *cb_arg,
		       uint32_t io_flags, uint32_t  option)
{
	struct nvme_request *req;
	struct nvme_payload payload;

	if (key_length > SAMSUNG_KV_MAX_KEY_SIZE) return -EINVAL;

	assert(0);//Not tested yet

	//payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	//payload.u.contig = buffer;

	payload.reset_sgl_fn = NULL;
	payload.contig_or_cb_arg = buffer;

	payload.md = keys; //Bugbug: how to determine the length of keys buffer in case of variable lengths

	req = _nvme_kv_cmd_allocate_request(ns, qpair, &payload, buffer_length,
					    0, 0, cb_fn, cb_arg, SPDK_NVME_OPC_SAMSUNG_KV_EXIST,
					    io_flags);

	if (NULL == req) {
		return -ENOMEM;
	}

	_nvme_kv_cmd_setup_exist_request(ns, req,
					 key_length, key_number, buffer_length,
					 io_flags, option);

	return nvme_qpair_submit_request(qpair, req);

}

/**
 * \brief Submits a KV Iterate I/O to the specified NVMe namespace.
 *
 * \param ns NVMe namespace to submit the KV Iterate I/O
 * \param qpair I/O queue pair to submit the request
 * \param bitmask pointer to Iterator bitmask (4 bytes)
 * \param iterator pointer to Iterator (3 bytes)
 * \param buffer virtual address pointer to the return buffer
 * \param buffer_length length (in bytes) of the return buffer
 * \param cb_fn callback function to invoke when the I/O is completed
 * \param cb_arg argument to pass to the callback function
 * \param io_flags set flags, defined by the SPDK_NVME_IO_FLAGS_* entries
 *                      in spdk/nvme_spec.h, for this I/O.
 * \param option option to pass to NVMe command
 *       0 - Fixed size; 1 - Variable size
 *
 * \return 0 if successfully submitted, ENOMEM if an nvme_request
 *           structure cannot be allocated for the I/O request, EINVAL if
 *           key_length or buffer_length is too large.
 *
 * The command is submitted to a qpair allocated by spdk_nvme_ctrlr_alloc_io_qpair().
 * The user must ensure that only one thread submits I/O on a given qpair at any given time.
 */
int
spdk_nvme_kv_cmd_iterate(struct spdk_nvme_ns *ns, struct spdk_nvme_qpair *qpair,
			 uint8_t *bitmask, uint8_t *iterator,
			 void *buffer, uint32_t buffer_length,
			 spdk_nvme_cmd_cb cb_fn, void *cb_arg,
			 uint32_t io_flags, uint32_t  option)
{
	struct nvme_request *req;
	struct nvme_payload payload;

	//payload.type = NVME_PAYLOAD_TYPE_CONTIG;
	//payload.u.contig = buffer;

	payload.reset_sgl_fn = NULL;
	payload.contig_or_cb_arg = buffer;

	payload.md = NULL;

	req = _nvme_kv_cmd_allocate_request(ns, qpair, &payload, buffer_length,
					    0, 0, cb_fn, cb_arg, SPDK_NVME_OPC_SAMSUNG_KV_ITERATE_CTRL,
					    io_flags);

	if (NULL == req) {
		return -ENOMEM;
	}

	_nvme_kv_cmd_setup_iterate_request(ns, req,
					   bitmask, iterator, buffer_length,
					   io_flags, option);

	return nvme_qpair_submit_request(qpair, req);

}
