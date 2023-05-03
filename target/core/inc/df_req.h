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


#ifndef DRAGONFLY_REQ_H
#define DRAGONFLY_REQ_H

/**
 * \file
 * dragonfly request definitions
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "spdk/stdinc.h"
#include "spdk/queue.h"
#include "spdk/event.h"

#include "spdk/nvmf_spec.h"

#include "df_def.h"
#include "df_poller.h"

#include "apis/dss_module_apis.h"

struct dfly_key {
	void     *key;
	uint16_t length;
};

struct dfly_value {
	void     *value;
	uint32_t length;
	int32_t  offset;
};


typedef struct dfly_list_info_s {
	int pe_cnt_tbd;		//nr of p/e pair to be processed.
	int pe_total_cnt;	//total nr of prefix/entry pairs from the key path.
	int16_t prefix_key_info[SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1];
	int list_size;			// list buffer size
	uint16_t list_zone_idx;	// idx of zone which handles the list requst.
	uint16_t max_keys_requested;	// max number of list key to be returned
	uint16_t options;	// list the root/ from beginning.
	uint16_t start_key_offset;	// offset of the key payload to delimit the prefix and start_key
} dfly_list_info_t;

struct dss_list_read_process_ctx_s {
	struct dfly_request *parent_req;
	struct dfly_value *val;
	uint32_t *total_keys;
	uint32_t max_keys;
	uint32_t *key_sz;
	void *key;
	uint32_t rem_buffer_len;
	uint32_t repopulate:1;
    uint32_t is_list_direct:1;
	char delim;
	char prefix[SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1];
	char start[SAMSUNG_KV_MAX_FABRIC_KEY_SIZE + 1];
};

typedef enum df_lat_states_e {
	DF_LAT_REQ_START = 0,
	DF_LAT_READY_TO_EXECUTE,
	DF_LAT_COMPLETED_FROM_DRIVE,
	DF_LAT_RO_STARTED,
	DF_LAT_REQ_END,
	DF_LAT_NUM_STATES
} df_lat_states_t ;

struct df_io_lat_ticks {
	uint64_t tick_arr[DF_LAT_NUM_STATES];
};

#define DFLY_REQF_ANONYMOUS	((uint32_t)0x00000001)
#define DFLY_REQF_NVMF          ((uint32_t)0x00000002)
#define DFLY_REQF_NVME  	((uint32_t)0x00000004)
#define DFLY_REQF_META  	((uint32_t)0x00000008)
#define DFLY_REQF_DATA  	((uint32_t)0x00000010)
#define DFLY_REQF_TARGET_DATA  	((uint32_t)0x00000020) /**<target to target io for rebalancing, etc., */


//Dragonfly request Type
#define DFLY_REQ_OPC_STORE_FUSE2 (0xC2)
#define DFLY_REQ_OPC_DELETE_FUSE2 (0xC3)

struct df_dev_response_s {
	bool rc;
	int32_t opc;
	uint32_t cdw0;
	uint32_t nvme_sct;
	uint32_t nvme_sc;
};

struct dfly_request_ops {

	/**--------- For consumer like data distribution --------------**/

	/**
	* get command for the given target
	*/
	uint32_t (*get_command)(struct dfly_request *req);

	/**
	* get key for the given target
	*/
	struct dfly_key *(*get_key)(struct dfly_request *req);

	/**---------- For consumer WAL/Cache/Flush calls --------------**/

	/**
	* get value for the given target
	*/
	struct dfly_value *(*get_value)(struct dfly_request *req);

	/**
	* validate active dfly_request
	*/
	int (*validate_req)(struct dfly_request *req);

	void (*complete_req)(struct df_dev_response_s resp, void *arg);

};

struct dss_request_s {
	dss_request_opc_t opc;
	dss_request_rc_t status;
	//Common request context struct for all modules
	dss_module_req_ctx_t module_ctx[DSS_MODULE_END];
};

typedef struct dfly_request {
	dss_request_t common_req;
	uint32_t    flags; /**< request type */

	uint16_t
	src_core; /**< where the request intiated and this will e uded to complete the request at the end */
	uint16_t    tgt_core; /**< where the request being served */

	uint16_t    src_nsid; /**< exposed device for io */
	uint16_t    tgt_nsid; /**< where the data being served */

	uint32_t    io_seq_no; /**< io sequence number */

	uint32_t    qos_attrib; /**< qos hint */

	uint32_t    xfed_len; /**< xfed len so far */

	uint32_t    state;
	uint32_t    next_action;

	uint8_t     retry_count;
	bool		status;
	bool		abort_cmd;

	bool  data_direct;

	//From nvme command
	uint8_t nvme_opcode;

	struct dfly_qpair_s *dqpair;

	struct dfly_key req_key;
	struct dfly_value req_value;
	//struct dfly_value fuse1_value; /**< value for fuse f1 compare data */
	void         *req_ctx; /**<parent nvme or rdma request*/
	void 		 *io_device; /**<kvssd device>*/
	uint32_t	rsp_cdw0;
	uint32_t 	rsp_sct;
	uint32_t	rsp_sc;
	struct dfly_request *parent; /**<parent request incase of splitting*/
	struct dfly_request *next_req;	/**<next request connected to this req */
	struct dfly_request *f1_req;  /* used only for f2 */
	struct dfly_dev *dev; /**< the device this request belongs to */

	struct dfly_subsystem *req_dfly_ss;

	// This field is only for ustat usage. The ustat needs to be handled through the stats variable which is part of struct spdk_nvmf_ns.
	// Thus, we need a field keeping the disk/ns for each dfly_req
	// TODO:
	// This should be integrated with dfly_dev or other variable and be assigned in the io-distribution logic.
	void *disk;

	int    req_ssid;

	void *req_private;
	void *key_data_buf;

	struct dfly_request_ops	ops; /**< services exposed by this request */

	uint64_t     qos_tags[DFLY_QOS_ATTRS];
	struct timespec req_ts;
	int32_t	opc ;
	TAILQ_ENTRY(dfly_request)   qos_links[DFLY_QOS_ATTRS];
	TAILQ_ENTRY(dfly_request)	link; /**< request pool linkage */

	TAILQ_ENTRY(dfly_request)   lock_pending;

	TAILQ_ENTRY(dfly_request)	wal_pending; /**< request pool linkage for retry */

#ifdef WAL_DFLY_TRACK
	TAILQ_ENTRY(dfly_request)	wal_debuging; /**< request pool linkage for retry */
	int	cache_rc;
	int log_rc;
	int dump_flag;
	int nr_objects_inserted;
	int dump_blk;
	int zone_flush_flags;
	int wal_status;	// 0. initial 1 cache_insert 2. log_submitted. 3 log_completed
	void *wal_map_item;
#endif
	void *req_fuse_data;
	struct {
		uint32_t dfly_iter_index;
		uint32_t bitmask;	//iter_open command cdw13
		uint32_t iter_val;	//iter_open command cdw12
		uint32_t iter_option; //iter_open command cdw11
		void *dev_iter_info;
		void *internal_cb;
	} iter_data;
	struct {
		struct rdd_rdma_queue_s *q;
        uint64_t req_cuid;
		uint64_t payload_len;
		uint64_t cmem;
		uint64_t hmem;
		uint32_t ckey;
		uint32_t hkey;
		uint16_t qhandle;
		uint8_t  opc;
		TAILQ_ENTRY(dfly_request)	pending;
	} rdd_info;
	dfly_list_info_t list_data;
	struct dss_list_read_process_ctx_s lp_ctx;
	TAILQ_ENTRY(dfly_request)	fuse_delay; /**< request pool linkage for retry */
	TAILQ_ENTRY(dfly_request)
	fuse_pending; /**< request linkage in process, to be unlinked on completion */
	TAILQ_ENTRY(dfly_request)	fuse_waiting; /**< F1 request waiting linkage for F2 */
	TAILQ_ENTRY(dfly_request)	fuse_pending_list;

	TAILQ_ENTRY(dfly_request)	outstanding;
	int32_t waiting_for_buffer:1;
	struct df_io_lat_ticks lat;
    uint64_t submit_tick;
    int print_to;
} dfly_request_t;

int dfly_req_ini(struct dfly_request *req, int flags, void *ctx);
int dfly_req_fini(struct dfly_request *req);

void dfly_nvmf_req_init(struct spdk_nvmf_request *req);

void dfly_fuse_release(struct dfly_request *request);


/**--------- For consumer like data distribution --------------**/

/**
* validate if a active dfly_request
*/

int dfly_req_validate(struct dfly_request *req);

/**
 * Get nvme cmd
 */
struct spdk_nvme_cmd *dfly_get_nvme_cmd(struct dfly_request *req);

/**
* get command for the given target
*/
uint32_t dfly_req_get_command(struct dfly_request *req);

/**
* get key for the given target
*/
struct dfly_key *dfly_req_get_key(struct dfly_request *req);

/**---------- For consumer WAL/Cache/Flush calls --------------**/

/**
* get value for the given target
*/
struct dfly_value *dfly_req_get_value(struct dfly_request *req);

void dfly_resp_set_cdw0(struct dfly_request *req, uint32_t value);
uint32_t dfly_resp_get_cdw0(struct dfly_request *req);


void dfly_req_init_nvmf_info(struct dfly_request *req);
void dfly_req_init_nvmf_value(struct dfly_request *req);

void dfly_req_init_nvme_info(struct dfly_request *req);

bool dfly_cmd_sequential(struct dfly_request *req1, struct dfly_request *req2);
void dfly_set_status_code(struct dfly_request *req, int sct, int sc);

void dss_set_rdd_transfer(struct dfly_request *req);
uint32_t dss_req_get_val_len(dss_request_t *req);

#ifdef __cplusplus
}
#endif

#endif // DRAGONFLY_REQ_H
