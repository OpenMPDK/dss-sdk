/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2021 Samsung Electronics Co., Ltd.
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
#include <string.h>

#include "spdk/stdinc.h"

#include "spdk/env.h"
#include "spdk/event.h"
#include "rdd.h"

#define RDD_DEFAULT_START_TEST_POLL_PERIOD_IN_US (2000000)

static struct option g_rdd_cmdline_options[] = {
#define RDD_IP_OPT_IDX	300
	{"rdd_ip",			required_argument,	NULL, RDD_IP_OPT_IDX},
#define RDD_PORT_OPT_IDX	301
	{"rdd_port",		required_argument,	NULL, RDD_PORT_OPT_IDX},
	{NULL,				optional_argument,	NULL, 302},
};

static const char *g_ip;
static const char *g_port;

enum queue_test_state_e {
	RDD_TEST_NOT_STARTED = 0,
	RDD_TEST_STARTED,
};

struct rdd_test_queue_ctx_s {
	enum queue_test_state_e state;
	struct rdd_rdma_queue_s *queuep;
	TAILQ_ENTRY(rdd_test_queue_ctx_s) link;
};

struct rdd_test_ctx_s {
	struct spdk_poller *test_start_poller;
	struct spdk_thread *test_th;
	int n_queues;
	int n_pend_queues;
	struct rdd_rdma_queue_s *last_queue_processed;
	TAILQ_HEAD( ,rdd_test_ctx_s) queues_started;
	TAILQ_HEAD( ,rdd_test_ctx_s) queues_pending;
	int done;
} g_rdd_test_ctx;

rdd_ctx_t *g_rdd_ctx;

void run_test(void *ctx);
int __start_all_test(void *arg);
void test_rdd_start_poller(void *arg);



void run_test(void *ctx)/*ctx --> queue*/
{
    struct rdd_rdma_queue_s *queue = (struct rdd_rdma_queue_s *)ctx;
    uint32_t vlen = 1024 * 1024;/*1MB*/

    void *cmem = NULL;
	uint32_t i = 0;
	dfly_request_t *req = NULL;

    // void *cmem = calloc(1, vlen);

    if(queue->th.spdk.cq_thread != spdk_get_thread()) {
        spdk_thread_send_msg(queue->th.spdk.cq_thread, run_test, ctx);
        return;
    }

	for(i=0; i < 4 * queue->send_qd; i++) {
		req = calloc(1, sizeof(dfly_request_t));
		cmem = (void *)spdk_zmalloc(vlen /*1MB*/,
                      0x1000 /*4k Align*/, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    	//rdd_post_cmd_host_read(queue, cmem, (void *)0xEDCBA987654321F0, vlen, NULL);
    	req->rdd_info.q = queue;
		req->rdd_info.opc = RDD_CMD_HOST_READ;
		req->rdd_info.cmem = (uint64_t) cmem;
		req->rdd_info.hmem = (uint64_t) 0xEDCBA987654321F0;
		req->rdd_info.payload_len = vlen;

		rdd_dss_submit_request((void *)req);

	}

    return;

}

int __start_all_test(void *arg)
{
	rdd_ctx_t *ctx = (rdd_ctx_t *)arg;
	struct rdd_rdma_queue_s *queue = NULL;

	if(TAILQ_EMPTY(&ctx->queues) || g_rdd_test_ctx.done == 1) {
		return SPDK_POLLER_IDLE;
	}

	//if(!g_rdd_test_ctx.last_queue_processed || 
	//		!(TAILQ_NEXT(g_rdd_test_ctx.last_queue_processed, link))) {
	//	return SPDK_POLLER_IDLE;
	//}

	if(!g_rdd_test_ctx.last_queue_processed) {
		queue = TAILQ_FIRST(&ctx->queues);
	} else {
		queue = TAILQ_NEXT(g_rdd_test_ctx.last_queue_processed, link);
	}

	if(queue) {
		if(queue->state != RDD_QUEUE_LIVE) {
			return SPDK_POLLER_IDLE;
		}

		DFLY_NOTICELOG("Queue %p processed\n", queue);
		spdk_thread_send_msg(queue->th.spdk.cq_thread, run_test, (void *)queue);
		g_rdd_test_ctx.n_queues++;

		g_rdd_test_ctx.last_queue_processed = queue;
		return SPDK_POLLER_BUSY;
	} else {
		g_rdd_test_ctx.done = 1;
	}

	return SPDK_POLLER_IDLE;
}

void _test_rdd_start_poller(void *arg)
{

	if(spdk_get_thread() != g_rdd_test_ctx.test_th) {
		spdk_thread_send_msg(g_rdd_test_ctx.test_th, _test_rdd_start_poller, arg);
		return;
	}

	g_rdd_test_ctx.n_pend_queues = 0;
	g_rdd_test_ctx.n_queues = 0;
	g_rdd_test_ctx.last_queue_processed = NULL;
	TAILQ_INIT(&g_rdd_test_ctx.queues_pending);
	TAILQ_INIT(&g_rdd_test_ctx.queues_started);
	g_rdd_test_ctx.done = 0;

	g_rdd_test_ctx.test_start_poller = SPDK_POLLER_REGISTER(__start_all_test, arg, RDD_DEFAULT_START_TEST_POLL_PERIOD_IN_US);
	DFLY_ASSERT(g_rdd_test_ctx.test_start_poller != NULL);

	return;
}

void test_rdd_start_poller(void *arg)
{
	rdd_ctx_t *ctx = (rdd_ctx_t *)arg;

	if(spdk_get_thread() != ctx->th.spdk.cm_thread) {
		spdk_thread_send_msg(ctx->th.spdk.cm_thread, test_rdd_start_poller, arg);
		return;
	}

	g_rdd_test_ctx.test_th = spdk_thread_create("test_th", NULL);
	DFLY_ASSERT(g_rdd_test_ctx.test_th);

	_test_rdd_start_poller(arg);

}

static void
rdd_test_usage(void)
{
	SPDK_ERRLOG("IP and port parameters required\n");
}

static int
rdd_test_parse_arg(int ch, char *arg)
{
	switch(ch) {
		case RDD_IP_OPT_IDX:
			g_ip = strdup(arg);
			break;
		case RDD_PORT_OPT_IDX:
			g_port = strdup(arg);
			break;
		default:
			break;
	}
	//SPDK_NOTICELOG("Param: %d [%s]\n", ch, arg);
	return 0;
}

static void
rdd_test_server_start(void *arg1)
{
	rdd_params_t params = {};

	if(!g_ip || !g_port) {
		rdd_test_usage();
		exit(1);
	}

	g_rdd_ctx = rdd_init(g_ip, g_port, params);
	if(!g_rdd_ctx) {
		SPDK_ERRLOG("RDD init failed\n");
		exit(1);
	}

	test_rdd_start_poller(g_rdd_ctx);
	
}

int main(int argc, char **argv)
{
	struct spdk_app_opts opts ={};
	int rc;

	// spdk_env_opts_init(&opts);
	// opts.name = "test_rdd";
	// opts.shm_id = 0;

	// if (spdk_env_init(&opts) < 0) {
	// 	fprintf(stderr, "Unable to initialize SPDK env\n");
	// 	return 1;
	// }

	/* default value in opts */
	spdk_app_opts_init(&opts);
	opts.name = "test_rdd";
	if ((rc = spdk_app_parse_args(argc, argv, &opts, "", g_rdd_cmdline_options,
				      rdd_test_parse_arg, rdd_test_usage)) !=
	    SPDK_APP_PARSE_ARGS_SUCCESS) {
		exit(rc);
	}

	/* Blocks until the application is exiting */
	rc = spdk_app_start(&opts, rdd_test_server_start, NULL);
	if(g_rdd_ctx) {
		rdd_destroy(g_rdd_ctx);
	}
	spdk_app_fini();

	return 0;
}
