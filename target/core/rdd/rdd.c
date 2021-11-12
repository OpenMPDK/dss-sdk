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

#include "rdd.h"

static int
rdd_rdma_mem_notify(void *cb_ctx, struct spdk_mem_map *map,
		     enum spdk_mem_map_notify_action action,
		     void *vaddr, size_t size)
{
	struct ibv_pd *pd = cb_ctx;
	struct ibv_mr *mr;
	int rc;

	switch (action) {
	case SPDK_MEM_MAP_NOTIFY_REGISTER:
			mr = ibv_reg_mr(pd, vaddr, size,
					IBV_ACCESS_LOCAL_WRITE |
					IBV_ACCESS_REMOTE_READ |
					IBV_ACCESS_REMOTE_WRITE);
			if (mr == NULL) {
				DFLY_ERRLOG("ibv_reg_mr() failed\n");
				return -1;
			} else {
				rc = spdk_mem_map_set_translation(map, (uint64_t)vaddr, size, (uint64_t)mr);
			}
		break;
	case SPDK_MEM_MAP_NOTIFY_UNREGISTER:
		mr = (struct ibv_mr *)spdk_mem_map_translate(map, (uint64_t)vaddr, NULL);
		if (mr) {
			ibv_dereg_mr(mr);
		}
		break;
	default:
		//SPDK_UNREACHABLE();
        abort();
	}

	return rc;
}

static int
rdd_rdma_check_contiguous_entries(uint64_t addr_1, uint64_t addr_2)
{
	/* Two contiguous mappings will point to the same address which is the start of the RDMA MR. */
	return addr_1 == addr_2;
}

const struct spdk_mem_map_ops g_rdd_rdma_map_ops = {
	.notify_cb = rdd_rdma_mem_notify,
	.are_contiguous = rdd_rdma_check_contiguous_entries
};

int rdd_cl_queue_established(rdd_ctx_t *ctx, struct rdma_cm_event *ev)
{
    struct rdma_cm_id *id = ev->id;
    struct rdd_rdma_queue_s *queue = (struct rdd_rdma_queue_s *)id->context;

    DFLY_NOTICELOG("qid %u connection estalished\n", queue->host_qid);

    queue->state = RDD_QUEUE_LIVE;

    TAILQ_INSERT_TAIL(&ctx->queues, queue, link);

    //TODO: Move out to test file
    //run_test((void *)queue);

    return 0;
}

struct rdd_req_s *rdd_get_free_request(struct rdd_rdma_queue_s *q)
{
    struct rdd_req_s *req;

    if(TAILQ_EMPTY(&q->free_reqs)) {
        return NULL;
    }

    req = TAILQ_FIRST(&q->free_reqs);
    TAILQ_REMOVE(&q->free_reqs, req, link);

    TAILQ_INSERT_TAIL(&q->outstanding_reqs, req, link);

    return req;
}

void rdd_put_free_request(struct rdd_req_s *req)
{
    DFLY_ASSERT(req != NULL);

    TAILQ_REMOVE(&req->q->outstanding_reqs, req, link);

    //Temporary freeing of source address
    //TODO: This should be done by the one submitting the request??
    spdk_free((void *)req->rdd_cmd->cmd.hread.caddr);

    TAILQ_INSERT_TAIL(&req->q->free_reqs, req, link);

    ibv_post_recv(req->q->qp, &req->q->rsps[req->rsp_idx].recv_wr, NULL);

    return;
}

//Run in same thread as the queue is polled
int rdd_post_cmd_host_read(struct rdd_rdma_queue_s *q, void *cmem, void *hmem, uint32_t vlen, void *ctx)
{
    int rc;
    //int translation_len = 0;
    struct rdd_req_s *req;

    struct ibv_mr *data_mr = NULL;
    struct ibv_send_wr *bad_wr = NULL;

    if(vlen > RDD_RDMA_MAX_KEYED_SGL_LENGTH) {
        DFLY_NOTICELOG("Unsupported value length\n");
        return -1;
    }

    req = rdd_get_free_request(q);
    if(!req) {
        assert(0);
        return -1;
    }
    
    //TODO: Update command
    req->rdd_cmd->opc = RDD_CMD_HOST_READ;
    req->rdd_cmd->cmd.hread.len = vlen;
    req->rdd_cmd->cmd.hread.caddr = (uint64_t) cmem;
    data_mr = (struct ibv_mr *)spdk_mem_map_translate(q->map, (uint64_t)cmem, NULL);
    req->rdd_cmd->cmd.hread.ckey = data_mr->rkey;
    //DFLY_ASSERT(translation_len >= vlen);
    req->rdd_cmd->cmd.hread.haddr = (uint64_t) hmem;// TODO: from host

    DFLY_NOTICELOG("Sending Command\n");
    rc = ibv_post_send(q->qp, &req->req.send_wr, &bad_wr);
    assert(rc ==0);
    assert(bad_wr == NULL);
    //TODO: Handle error??

    return rc;
}

int rdd_process_resp(struct rdd_rdma_rsp_s *rsp)
{
    return 0;
}

int rdd_process_wc(struct ibv_wc *wc)
{
    struct rdd_req_s *req = NULL;
    struct rdd_rsp_s *rsp;
    struct rdd_wr_s *rdd_wr = (struct rdd_wr_s *)wc->wr_id;

    DFLY_NOTICELOG("Got wc with opcode %d and status %s\n", wc->opcode, ibv_wc_status_str(wc->status));

    if(wc->status != IBV_WC_SUCCESS) {
        DFLY_ERRLOG("wc error %d\n", wc->status);
        return -1;
    }

    switch(rdd_wr->type) {
        case RDD_WR_TYPE_REQ_SEND:
            //TODO: Host recieved SEND??
            DFLY_NOTICELOG("Send acknowledgement recieved\n");
            break;
        case RDD_WR_TYPE_RSP_RECV:
            rsp = SPDK_CONTAINEROF(rdd_wr, struct rdd_rsp_s, rdd_wr);
            req = &rsp->q->reqs[rsp->rsp.cid];
            req->rsp_idx = rsp->idx;

            DFLY_NOTICELOG("comand completion recieved opc %d cid %d status %d\n", req->rdd_cmd->opc, rsp->rsp.cid, rsp->rsp.status);
            rdd_put_free_request(req);
            //ibv_post_recv(rsp->q->qp, &rsp->recv_wr, NULL);
            //TODO: Handle reture code
            break;
        default:
            DFLY_NOTICELOG("Unknown WC type recieved\n");
            break;
    }

    return 0;
}

int rdd_cq_poll(void *arg)
{
    struct rdd_rdma_queue_s *queue = (struct rdd_rdma_queue_s *)arg;
    //struct ibv_comp_channel * ch = queue->comp_ch;

    struct ibv_cq *cq;
    struct ibv_wc wc;
    //void *ctx;

    int rc;

    //ibv_get_cq_event(ch, &cq, &ctx);
    //ibv_ack_cq_events(cq, 1);
    //ibv_req_notify_cq(cq, 0);

    //TODO: Handle multiple wc in one poll
	if ((rc = ibv_poll_cq(queue->cq, 1, &wc)) > 0) {
        rc = rdd_process_wc(&wc);
        if(rc) {
            //Error processing WC
            //TODO: Handle error
        }
        return SPDK_POLLER_BUSY;
  	} else {
        if(rc <0) {
            DFLY_ERRLOG("Rdma cq poll failed for %p\n", queue);
            //TODO: Error handling
        }
    }
	return SPDK_POLLER_IDLE;
}

static void _rdd_start_cq_poller(void *arg)
{
    struct rdd_rdma_queue_s *queue = (struct rdd_rdma_queue_s *)arg;

    DFLY_ASSERT(spdk_get_thread() == queue->th.spdk.cq_thread);
    DFLY_ASSERT(queue->th.spdk.cq_poller == NULL);

    queue->th.spdk.cq_poller = SPDK_POLLER_REGISTER(rdd_cq_poll, queue, RDD_DEFAULT_CM_POLL_PERIOD_IN_US);
    DFLY_ASSERT(queue->th.spdk.cq_poller != NULL);

    DFLY_NOTICELOG("Started cq poll thread for queue %p on core %d \n", queue, spdk_env_get_current_core());

    return;
}

//TODO: Run on core where the ib queue needs to be polled
static int rdd_queue_ib_create(struct rdd_rdma_queue_s *queue)
{
    struct ibv_cq *cq;
    struct ibv_qp_init_attr qp_attr;
    struct rdma_cm_id *id = queue->cm_id;

    int cqc;
    char thread_name[256] = {0};
    int rc = 0;
    int i;

    DFLY_ASSERT(queue->comp_ch == NULL);
    queue->comp_ch = ibv_create_comp_channel(id->verbs);

    DFLY_ASSERT(queue->comp_ch);
    //TODO: Check and fail on error

    cqc = queue->recv_qd + 2 * queue->send_qd;
    //TODO: Verify this calculation is valid for this queue

    cq = ibv_create_cq(id->verbs, cqc, NULL, queue->comp_ch,
                    queue->host_qid % id->verbs->num_comp_vectors);
    DFLY_ASSERT(cq);

    queue->cq = cq;
    queue->ncqe = cqc;

    //TODO: Check and fail on error
    ibv_req_notify_cq(cq, 0);

    DFLY_ASSERT(queue->th.spdk.cq_thread == NULL);
    sprintf(thread_name, "rdd_qp_%p", queue);
    //Created thread in the current core
    queue->th.spdk.cq_thread = spdk_thread_create(thread_name, NULL);
    if(!queue->th.spdk.cq_thread) {
        DFLY_ERRLOG("Failed to create cq poll thread %s\n", thread_name);
        DFLY_ASSERT(0);//To do call destroy function and handle
        //rdd_destroy(ctx);
        return NULL;
    }

    spdk_thread_send_msg(queue->th.spdk.cq_thread, _rdd_start_cq_poller, (void *)queue);
    //TODO: Wait for initialization

    memset(&qp_attr, 0, sizeof(struct ibv_qp_init_attr));

    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;

    qp_attr.cap.max_send_wr = 2 * queue->send_qd;
    qp_attr.cap.max_recv_wr = queue->recv_qd;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    rc = rdma_create_qp(id, queue->pd, &qp_attr);
    DFLY_ASSERT(rc == 0);
    //TODO: Handle error case

    queue->qp = id->qp;

    for(i=0; i<queue->send_qd; i++) {
        struct rdd_rsp_s *rsp = &queue->rsps[i];
        
        rc = ibv_post_recv(queue->qp, &rsp->recv_wr, NULL);
        DFLY_ASSERT(rc == 0);
        //TODO: Handle error conditions
    }

    return rc;
}

static int rdd_queue_alloc_reqs(struct rdd_rdma_queue_s *queue)
{
    int i;

    queue->reqs = (struct rdd_req_s *)calloc(queue->send_qd, sizeof(struct rdd_req_s));
    queue->cmds = (rdd_rdma_cmd_t *)calloc(queue->send_qd, sizeof(rdd_rdma_cmd_t));
    queue->rsps = (struct rdd_rsp_s *)calloc(queue->send_qd, sizeof(struct rdd_rsp_s));

    if(!queue->reqs || !queue->cmds || !queue->rsps) {
        DFLY_ERRLOG("Failed to alloc requests for queue %p\n", queue);
        //rdd_queue_destroy(queue);//TODO: ON return of error code
        return -1;
    }

    queue->cmd_mr = ibv_reg_mr(queue->pd, queue->cmds,
                        queue->send_qd * sizeof(rdd_rdma_cmd_t), 
         					IBV_ACCESS_LOCAL_WRITE |
						    IBV_ACCESS_REMOTE_WRITE |
							IBV_ACCESS_REMOTE_READ);                   

    queue->rsp_mr = ibv_reg_mr(queue->pd, queue->rsps,
                        queue->send_qd * sizeof(struct rdd_rsp_s), 
         					IBV_ACCESS_LOCAL_WRITE |
						    IBV_ACCESS_REMOTE_WRITE |
							IBV_ACCESS_REMOTE_READ);          

    if(!queue->cmd_mr || !queue->rsp_mr) {
        DFLY_ERRLOG("Failed to register memory regions for queue %p error %d\n", queue, errno);
        //rdd_queue_destroy(queue);//TODO: ON return of error code
        return -1;
    }         

    for(i=0; i< queue->send_qd; i++) {
        queue->reqs[i].rdd_cmd = &queue->cmds[i];

        queue->reqs[i].rdd_cmd->cid = i;

        queue->reqs[i].req.send_sge.addr = (uint64_t)queue->reqs[i].rdd_cmd;
        queue->reqs[i].req.send_sge.length = sizeof(rdd_rdma_cmd_t);
        queue->reqs[i].req.send_sge.lkey = queue->cmd_mr->lkey;

        queue->reqs[i].req.send_wr.opcode = IBV_WR_SEND;
        queue->reqs[i].req.rdd_wr.type    = RDD_WR_TYPE_REQ_SEND;
        queue->reqs[i].req.send_wr.wr_id = (uint64_t)&queue->reqs[i].req.rdd_wr;
        queue->reqs[i].req.send_wr.sg_list = &queue->reqs[i].req.send_sge;
        queue->reqs[i].req.send_wr.num_sge = 1;
        queue->reqs[i].req.send_wr.next = NULL;
        queue->reqs[i].req.send_wr.send_flags = IBV_SEND_SIGNALED;
        queue->reqs[i].req.send_wr.imm_data = 0;
        queue->reqs[i].id = i;
        queue->reqs[i].rsp_idx = -1;

        queue->rsps[i].rdd_wr.type   = RDD_WR_TYPE_RSP_RECV;
        queue->rsps[i].recv_sge.addr = (uint64_t)&queue->rsps[i].rsp;
        queue->rsps[i].recv_sge.length = sizeof(rdd_rdma_rsp_t);
        queue->rsps[i].recv_sge.lkey = queue->rsp_mr->lkey;
        queue->rsps[i].idx = i;

        queue->rsps[i].recv_wr.wr_id = (uint64_t)&queue->rsps[i].rdd_wr;
        queue->rsps[i].recv_wr.sg_list = &queue->rsps[i].recv_sge;
        queue->rsps[i].recv_wr.num_sge = 1;
        queue->rsps[i].recv_wr.next = NULL;
        queue->rsps[i].q = queue;

        queue->reqs[i].state = RDD_REQ_FREE;
        queue->reqs[i].q = queue;

        TAILQ_INSERT_TAIL(&queue->free_reqs, &queue->reqs[i], link);
    }

    return 0;
}

int rdd_queue_accept(rdd_ctx_t *ctx, struct rdma_cm_event *ev)
{
    struct rdma_cm_id *id = ev->id;
    struct rdma_conn_param conn_param = {};
    struct rdma_conn_param *cl_conn_param = &ev->param.conn;
    struct ibv_device_attr cl_dev_attr;

    struct rdd_queue_priv_s priv = {};

    int ret;

    ret = ibv_query_device(id->verbs, &cl_dev_attr);
    if(ret) {
        DFLY_ERRLOG("Failed to query device attribute %s\n", id->verbs->device->name);
        return -1;
    }

    priv.data.server.proto_ver = RDD_PROTOCOL_VERSION;
    priv.data.server.qhandle = 0x12345678;//TODO: queue handle should be generated

    conn_param.rnr_retry_count = 7;
    conn_param.flow_control = 1;
    if((uint8_t)cl_dev_attr.max_qp_init_rd_atom < cl_conn_param->initiator_depth) {
        conn_param.initiator_depth = (uint8_t)cl_dev_attr.max_qp_init_rd_atom;
    } else {
        conn_param.initiator_depth = cl_conn_param->initiator_depth;
    }
    conn_param.responder_resources = conn_param.initiator_depth;
    conn_param.private_data = &priv;
    conn_param.private_data_len = sizeof(priv);

    ret = rdma_accept(id, &conn_param);
    if(ret) {
        DFLY_ERRLOG("rdma_accept failed error code (%d) \n", ret);
        return -1;
    }

    return 0;
}

int rdd_queue_connect(rdd_ctx_t *ctx, struct rdma_cm_event *ev)
{
    struct rdma_cm_id *id = ev->id;
    struct rdd_queue_priv_s *priv = (struct rdd_queue_priv_s *)ev->param.conn.private_data;

    struct rdd_rdma_queue_s *queue = NULL;

    int rc = 0;

    //DFLY_ASSERT(ctx->tr.rdma.cm_id == id);//TODO: This could be differnt
    DFLY_ASSERT(spdk_get_thread() == ctx->th.spdk.cm_thread);

    if(priv->data.client.proto_ver != RDD_PROTOCOL_VERSION) {
        DFLY_ERRLOG("Unsupported client version %x\n", priv->data.client.proto_ver);
        return -1;
    }

    queue = (struct rdd_rdma_queue_s *)calloc(1, sizeof(struct rdd_rdma_queue_s));
    if(!queue) {
        DFLY_ERRLOG("Failed to allocate queue\n");
        return -1;
    }

    queue->ctx = ctx;
    queue->cm_id = id;
    queue->host_qid = priv->data.client.qid;
    queue->recv_qd = priv->data.client.hsqsize;
    queue->send_qd = priv->data.client.hrqsize;
    queue->state = RDD_QUEUE_CONNECTING;

    TAILQ_INIT(&queue->free_reqs);
    TAILQ_INIT(&queue->outstanding_reqs);

    queue->pd = ibv_alloc_pd(id->verbs);
    DFLY_ASSERT(queue->pd);

    queue->map = spdk_mem_map_alloc(0, &g_rdd_rdma_map_ops, queue->pd);
    if(!queue->map) {
        //TODO: Error handling
        DFLY_ASSERT(queue->map);
    }

    id->context = queue;//Got back on connection establishment from client

    rc = rdd_queue_alloc_reqs(queue);
    if(rc) {
        rdd_queue_destroy(queue);
        return -1;
    }

    rc = rdd_queue_ib_create(queue);
    if(rc) {
        rdd_queue_destroy(queue);
        return -1;
    }

    rc = rdd_queue_accept(ctx, ev);
    DFLY_ASSERT(rc == 0);
    
    return 0;
}

int rdd_queue_destroy(struct rdd_rdma_queue_s *queue)
{
    int rc = 0;

    //TODO: Dealloc ibv queues
    DFLY_ASSERT(0);


    if(queue->pd) {
        rc = ibv_dealloc_pd(queue->pd);
        DFLY_ASSERT(rc == 0);
        //TODO: handle error scenario
    }

    if(queue->cmds) {
        free(queue->cmds);
        queue->cmds = NULL;
    }

    if(queue->rsps) {
        free(queue->rsps);
        queue->rsps = NULL;
    }

    if(queue->reqs) {
        free(queue->reqs);
        queue->reqs = NULL;
    }

    return rc;
}

int rdd_cm_event_handler(rdd_ctx_t *ctx, struct rdma_cm_event *ev)
{
	int r = 0;

    DFLY_NOTICELOG("Processing event %s\n", rdma_event_str(ev->event));
	switch (ev->event) {
        case RDMA_CM_EVENT_CONNECT_REQUEST:
            r = rdd_queue_connect(ctx, ev);
            //Connect queue
            break;
        case RDMA_CM_EVENT_ESTABLISHED:
            r = rdd_cl_queue_established(ctx, ev);
            break;
	    default:
		    DFLY_ERRLOG("Unhandled event %d for %p\n", ev->event, ctx);
	}
	return r;    
}

int rdd_cm_event_task(void *arg)
{
    rdd_ctx_t *ctx = (rdd_ctx_t *)arg;

	struct rdma_event_channel *ch = ctx->tr.rdma.ev_channel;
	struct rdma_cm_event *event;
    int rc;

    rc = poll(&ctx->th.spdk.cm_poll_fd, 1, 0);
    if (rc == 0) {
        return SPDK_POLLER_IDLE;
    }

    if(rc < 0) {
        //TODO: Handle Error
        DFLY_ERRLOG("cm event poll failed with error [%d]: %s\n", errno, spdk_strerror(errno));
    }

    while(1) {
	    rc = rdma_get_cm_event(ch, &event);
        if (rc) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
                DFLY_ERRLOG("Rdma event retrieval failed for %p with error [%d]: [%s]\n", ctx, errno, spdk_strerror(errno));
			}
			break;
		}
        if(rdd_cm_event_handler(ctx, event)) {
            DFLY_ERRLOG("CM event handler failed for %p event %p\n", ctx, event);
                //TODO: Connect Error stop poller and exit
        }
		rdma_ack_cm_event(event);
  	} 
	return SPDK_POLLER_BUSY;
}

static void _rdd_start_cm_event_poller(void *arg)
{
    rdd_ctx_t *ctx = (rdd_ctx_t *)arg;

    int flags, rc;

    flags = fcntl(ctx->tr.rdma.ev_channel->fd, F_GETFL);
    rc = fcntl(ctx->tr.rdma.ev_channel->fd, F_SETFL, flags | O_NONBLOCK);
    if (rc < 0) {
        DFLY_ERRLOG("Failed to change file descriptor of cm event channel\n");
        abort();
        return; //Handle error scenario
    }

    ctx->th.spdk.cm_poll_fd.fd = ctx->tr.rdma.ev_channel->fd;
    ctx->th.spdk.cm_poll_fd.events = POLLIN;
    ctx->th.spdk.cm_poll_fd.revents = 0;

    DFLY_ASSERT(spdk_get_thread() == ctx->th.spdk.cm_thread);
    DFLY_ASSERT(ctx->th.spdk.cm_poller == NULL);
    ctx->th.spdk.cm_poller = SPDK_POLLER_REGISTER(rdd_cm_event_task, ctx, RDD_DEFAULT_CM_POLL_PERIOD_IN_US);
    DFLY_ASSERT(ctx->th.spdk.cm_poller != NULL);

    DFLY_NOTICELOG("Started cm event poll thread for %p on core %d \n", ctx, spdk_env_get_current_core());
}

static void _rdd_stop_cm_event_poller(void *arg)
{
    rdd_ctx_t *ctx = (rdd_ctx_t *)arg;

    DFLY_ASSERT(ctx->th.spdk.cm_poller != NULL);
    DFLY_ASSERT(ctx->th.spdk.cm_thread != NULL);
    DFLY_ASSERT(spdk_get_thread() == ctx->th.spdk.cm_thread);

    spdk_poller_unregister(&ctx->th.spdk.cm_poller);
    ctx->th.spdk.cm_poller = NULL;
    spdk_thread_exit(ctx->th.spdk.cm_thread);
    ctx->th.spdk.cm_thread = NULL;

    return;
}

//Call this function on the core where the cm_event thread needs to run
rdd_ctx_t *rdd_init(const char *listen_ip, const char *listen_port, rdd_params_t params)
{
    rdd_ctx_t *ctx;
    char thread_name[256] = {0};

    struct addrinfo hints;
    int rc;

    ctx = (rdd_ctx_t *)calloc(1, sizeof(rdd_ctx_t));
    if(!ctx) {//Context alloc failed
        DFLY_ERRLOG("context alloc failed for ip %s port %s\n",
                listen_ip, listen_port);
        return NULL;
    }

    memset(&hints, 0, sizeof(hints));
    //hints.ai_family = AF_UNSPEC;//Try to resolve IPV4 or IPV6 address
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    ctx->listen_ip = strdup(listen_ip);
    ctx->listen_port = strdup(listen_port);

    TAILQ_INIT(&ctx->queues);

	rc = getaddrinfo(listen_ip, listen_port, &hints, &ctx->ai);
    if(rc) {
        DFLY_ERRLOG("Failed to get addrinfo ip %s port %s error '%s' (%d)\n", 
                        listen_ip, listen_port, gai_strerror(rc), rc);
        rdd_destroy(ctx);
        return NULL;
    }

    ctx->tr.rdma.ev_channel = rdma_create_event_channel();
    if(!ctx->tr.rdma.ev_channel) {
        DFLY_ERRLOG("rdma_create_event_channel failed for ip %s port %s %d\n",
                        listen_ip, listen_port, errno);
        rdd_destroy(ctx);
        return NULL;
    }

    rc = rdma_create_id(ctx->tr.rdma.ev_channel, &ctx->tr.rdma.cm_id, NULL, RDMA_PS_TCP);
    if (rc) {
        //rc == -1
        DFLY_ERRLOG("rdma_create_id failed for ip %s port %s %d\n",
                listen_ip, listen_port, errno);
        rdd_destroy(ctx);
        return NULL;
    }

    //TODO: bind/listen to all available addr in ai
    //Now binding only to the first
    rc = rdma_bind_addr(ctx->tr.rdma.cm_id, ctx->ai->ai_addr);
    if(rc) {
        //rc == -1
        DFLY_ERRLOG("rdma_bind_addr failed for ip %s port %s %d\n",
                listen_ip, listen_port, errno);
        rdd_destroy(ctx);
        return NULL;
    }

    rc = rdma_listen(ctx->tr.rdma.cm_id, RDD_DEFAULT_LISTEN_BACKLOG);
    if(rc) {
        //rc == -1
        DFLY_ERRLOG("rdma_listen failed for ip %s port %s %d\n",
                listen_ip, listen_port, errno);
        rdd_destroy(ctx);
        return NULL;
    }

    sprintf(thread_name, "rdd_%p", ctx->tr.rdma.cm_id);
    //Created thread in the current core
    ctx->th.spdk.cm_thread = spdk_thread_create(thread_name, NULL);
    if(!ctx->th.spdk.cm_thread) {
        DFLY_ERRLOG("Failed to create cm event thread %s\n", thread_name);
        rdd_destroy(ctx);
        return NULL;
    }

    spdk_thread_send_msg(ctx->th.spdk.cm_thread, _rdd_start_cm_event_poller, (void *)ctx);
    //TODO: Wait for initialization

    DFLY_NOTICELOG("Listening on IP %s port %s\n", listen_ip, listen_port);
    return ctx;
}

void rdd_destroy(rdd_ctx_t *ctx)
{
    int rc;

    DFLY_NOTICELOG("Stop Listening on IP %s port %s\n", ctx->listen_ip, ctx->listen_port);

    if(ctx->th.spdk.cm_thread) {
        spdk_thread_send_msg(ctx->th.spdk.cm_thread, _rdd_stop_cm_event_poller, (void *) ctx);
        //TODO: Synchronize Stop thread and poller
    }

    if(ctx->tr.rdma.ev_channel) {
        rdma_destroy_event_channel(ctx->tr.rdma.ev_channel);
    }

    if(ctx->tr.rdma.cm_id) {
        rc = rdma_destroy_id(ctx->tr.rdma.cm_id);
        if(!rc) {
            //rc == -1
            //ALL QP need to be free and all events acked before destroy
            DFLY_ERRLOG("rdma_destroy_id failed for ip %s port %s %d\n",
                ctx->listen_ip, ctx->listen_port, errno);
        }
    }
    
    if(ctx->ai) {
        freeaddrinfo(ctx->ai);
    }

    if(ctx->listen_ip) {
        free(ctx->listen_ip);
    }

    if(ctx->listen_port) {
        free(ctx->listen_port);
    }

    if(ctx) {
        free(ctx);
    }
}