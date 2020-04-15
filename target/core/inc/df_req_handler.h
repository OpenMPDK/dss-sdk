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




typedef enum dfly_req_state_e {
	DFLY_REQ_BEGIN = 0,				// 0
	DFLY_REQ_INITIALIZED,			// 1
	DFLY_REQ_IO_SUBMITTED_TO_FUSE,	// 2
	DFLY_REQ_IO_SUBMITTED_TO_WAL,	// 3
	DFLY_REQ_IO_INTERNAL_SUBMITTED_TO_WAL,	// 4
	DFLY_REQ_IO_WRITE_MISS_SUBMITTED_TO_LOG,	// 5
	DFLY_REQ_IO_SUBMITTED_TO_DEVICE,		// 6
	DFLY_REQ_IO_COMPLETED_FROM_DEVICE,		// 7
	DFLY_REQ_IO_LIST_FORWARD,		// 8
	DFLY_REQ_IO_NVMF_DONE,			//9
	DFLY_REQ_FINISH,
} dfly_req_state_t;

typedef enum dfly_req_next_action_e {
	DFLY_ACTION_NONE = 0,
	DFLY_FORWARD_TO_IO_THREAD,			// 1
	DFLY_FORWARD_TO_WAL,				// 2
	DFLY_FORWARD_TO_QOS,				// 3
	DFLY_COMPLETE_NVMF,					// 4
	DFLY_COMPLETE_NVME,					// 5
	DFLY_COMPLETE_ON_FUSE_THREAD,		// 6
	DFLY_REQ_IO_LIST_DONE,				// 7
	DFLY_COMPLETE_IO,					// 8
} dfly_req_next_action_t;

int dfly_handle_request(struct dfly_request *req);
