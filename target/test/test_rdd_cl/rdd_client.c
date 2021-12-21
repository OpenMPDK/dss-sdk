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

#include "rdd_cl.h"

#define _GNU_SOURCE
#include <sched.h>


int stick_this_thread_to_core(pthread_t th_h, int core_id) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   return pthread_setaffinity_np(th_h, sizeof(cpu_set_t), &cpuset);
}

struct rdd_cl_dev_s *rdd_cl_find_dev(struct rdd_client_ctx_s *cl_ctx, const char *name)
{
    struct rdd_cl_dev_s *dev;
    TAILQ_FOREACH(dev, &cl_ctx->devices, dev_link) {
        if (!strcmp(dev->dev->name, name))
			return dev;
    }
	return NULL;
}

static int rdd_cl_queue_alloc_reqs(rdd_cl_queue_t *queue)
{
    int i;

    queue->reqs = (struct rdd_cl_req_s *)calloc(queue->conn->qd, sizeof(struct rdd_cl_req_s));
    queue->cmds = (rdd_rdma_cmd_t *)calloc(queue->conn->qd, sizeof(rdd_rdma_cmd_t));
    queue->rsps = (rdd_rdma_rsp_t *)calloc(queue->conn->qd, sizeof(rdd_rdma_rsp_t));

    if(!queue->reqs || !queue->cmds || !queue->rsps) {
        printf("Failed to alloc requests for queue %p\n", queue);
        //TODO: ON return of error code
        return -1;
    }

    queue->cmd_mr = ibv_reg_mr(queue->pd, queue->cmds,
                        queue->conn->qd * sizeof(rdd_rdma_cmd_t), 
         					IBV_ACCESS_LOCAL_WRITE |
						    IBV_ACCESS_REMOTE_WRITE |
							IBV_ACCESS_REMOTE_READ);                   

    queue->rsp_mr = ibv_reg_mr(queue->pd, queue->rsps,
                        queue->conn->qd * sizeof(rdd_rdma_rsp_t), 
         					IBV_ACCESS_LOCAL_WRITE |
						    IBV_ACCESS_REMOTE_WRITE |
							IBV_ACCESS_REMOTE_READ);          

    if(!queue->cmd_mr || !queue->rsp_mr) {
        printf("Failed to register memory regions for queue %p error %d\n", queue, errno);
        //TODO: ON return of error code
        return -1;
    }         

    for(i=0; i< queue->conn->qd; i++) {
        queue->reqs[i].rdd_cl_cmd = &queue->cmds[i];
        queue->reqs[i].rdd_cl_rsp = &queue->rsps[i];

        queue->reqs[i].req.recv_sge.addr = (uint64_t)queue->reqs[i].rdd_cl_cmd;
        queue->reqs[i].req.recv_sge.length = sizeof(rdd_rdma_cmd_t);
        queue->reqs[i].req.recv_sge.lkey = queue->cmd_mr->lkey;

        queue->reqs[i].req.rdd_cl_wr.type    = RDD_CL_WR_TYPE_REQ_RECV;
        queue->reqs[i].req.recv_wr.wr_id = (uint64_t)&queue->reqs[i].req.rdd_cl_wr;
        queue->reqs[i].req.recv_wr.sg_list = &queue->reqs[i].req.recv_sge;
        queue->reqs[i].req.recv_wr.num_sge = 1;
        queue->reqs[i].req.recv_wr.next = NULL;

        queue->reqs[i].data.rdd_cl_wr.type = RDD_CL_WR_TYPE_DATA_READ;
        queue->reqs[i].data.data_wr.wr_id = (uint64_t)&queue->reqs[i].data.rdd_cl_wr;
        queue->reqs[i].data.data_wr.sg_list = &queue->reqs[i].data.data_sge;
        queue->reqs[i].data.data_wr.opcode = IBV_WR_RDMA_READ;//IBV_WR_RDMA_WRITE;//IBV_WR_RDMA_READ;//
        queue->reqs[i].data.data_wr.send_flags = IBV_SEND_SIGNALED;
        queue->reqs[i].data.data_wr.num_sge = 1;
        queue->reqs[i].data.data_wr.next = NULL;

        queue->reqs[i].rsp.send_wr.opcode = IBV_WR_SEND;
        queue->reqs[i].rsp.rdd_cl_wr.type   = RDD_CL_WR_TYPE_RSP_SEND;
        queue->reqs[i].rsp.send_sge.addr = (uint64_t)queue->reqs[i].rdd_cl_rsp;
        queue->reqs[i].rsp.send_sge.length = sizeof(rdd_rdma_rsp_t);
        queue->reqs[i].rsp.send_sge.lkey = queue->rsp_mr->lkey;
        queue->reqs[i].rsp.send_wr.send_flags = IBV_SEND_SIGNALED;

        queue->reqs[i].rsp.send_wr.wr_id = (uint64_t)&queue->reqs[i].rsp.rdd_cl_wr;
        queue->reqs[i].rsp.send_wr.sg_list = &queue->reqs[i].rsp.send_sge;
        queue->reqs[i].rsp.send_wr.num_sge = 1;
        queue->reqs[i].rsp.send_wr.next = NULL;

        queue->reqs[i].state = RDD_CL_REQ_FREE;

        queue->reqs[i].q = queue;

        //TODO: Free list ??
        //TAILQ_INSERT_TAIL(&queue->free_reqs, &queue->reqs[i], link);
    }

    return 0;
}

static int rdd_cl_init_ib_queue(rdd_cl_queue_t *q)
{
 	struct ibv_cq *cq;
 	struct ibv_qp_init_attr qp_attr;
 	struct rdma_cm_id *id = q->cm_id;
    rdd_cl_conn_ctx_t * conn = q->conn;

    int rc;
    
    //q->pd = ibv_alloc_pd(q->cm_id->verbs);
    //assert(q->pd != NULL);//TODO: Handle error case

  	cq = ibv_create_cq(id->verbs, RDD_CL_CQ_QDEPTH, NULL, conn->ch,
 			           q->qid % id->verbs->num_comp_vectors);
 	ibv_req_notify_cq(cq, 0);

 	memset(&qp_attr, 0, sizeof (struct ibv_qp_init_attr));

 	qp_attr.send_cq = cq;
 	qp_attr.recv_cq = cq;
 	qp_attr.qp_type = IBV_QPT_RC;

 	qp_attr.cap.max_send_wr = conn->qd;
   	qp_attr.cap.max_recv_wr = conn->qd;
 	qp_attr.cap.max_send_sge = 16;
 	qp_attr.cap.max_recv_sge = 16;

 	rc = rdma_create_qp(id, NULL, &qp_attr);
    assert(rc == 0);
 	q->qp = id->qp;
    q->pd = id->qp->pd;

     //TODO
     rc = rdd_cl_queue_alloc_reqs(q);
     //TODO: Handle error case

 	return 0;
}

int rdd_cl_process_wc(struct ibv_wc *wc)
{
    struct rdd_cl_wr_s *rdd_cl_wr = (struct rdd_cl_wr_s *)wc->wr_id;
    struct rdd_cl_req_s *rdd_cl_req = NULL;

    int rc = 0;

    struct ibv_send_wr *bad_wr = NULL;

    //fprintf(stdout, "Got wc opcode %d status %s\n", wc->opcode, ibv_wc_status_str(wc->status));

    if (wc->status != IBV_WC_SUCCESS) {
		fprintf(stderr, "wc error %d\n", wc->status);
        abort();
		return -1;
	}

    switch (rdd_cl_wr->type)
    {
    case RDD_CL_WR_TYPE_REQ_RECV:
        rdd_cl_req = containerof(rdd_cl_wr, struct rdd_cl_req_s, req);
        //fprintf(stdout,"Recieved cmd with cid %d opc %x state %x caddr %p ckey %x haddr %p len %x\n",
          //          rdd_cl_req->rdd_cl_cmd->cid, rdd_cl_req->rdd_cl_cmd->opc, rdd_cl_req->state,
            //        rdd_cl_req->rdd_cl_cmd->cmd.hread.caddr, rdd_cl_req->rdd_cl_cmd->cmd.hread.ckey,
              //      rdd_cl_req->rdd_cl_cmd->cmd.hread.haddr, rdd_cl_req->rdd_cl_cmd->cmd.hread.len);

        assert(rdd_cl_req->state == RDD_CL_REQ_FREE);
        rdd_cl_req->rdd_cl_rsp->cid = rdd_cl_req->rdd_cl_cmd->cid;
        rdd_cl_req->rdd_cl_rsp->status = 0;
        rdd_cl_req->state = RDD_CL_REQ_RECV;

        //TODO: Check max length supportred
        //rdd_cl_req->temp_data = calloc(1, rdd_cl_req->rdd_cl_cmd->cmd.hread.len);//TMP alloc
        //assert(rdd_cl_req->temp_data != NULL); //TMP
        rdd_cl_req->data_mr = ibv_reg_mr(rdd_cl_req->q->pd,
                                            //rdd_cl_req->temp_data,
                                            rdd_cl_req->rdd_cl_cmd->cmd.hread.haddr,
                                            rdd_cl_req->rdd_cl_cmd->cmd.hread.len,
                                            IBV_ACCESS_LOCAL_WRITE |
						                    IBV_ACCESS_REMOTE_WRITE |
							                IBV_ACCESS_REMOTE_READ); //TODO: Verify performance impact
        assert(rdd_cl_req->data_mr != NULL);
        rdd_cl_req->data.data_wr.wr.rdma.remote_addr = rdd_cl_req->rdd_cl_cmd->cmd.hread.caddr;
        rdd_cl_req->data.data_wr.wr.rdma.rkey = rdd_cl_req->rdd_cl_cmd->cmd.hread.ckey;
        //rdd_cl_req->data.data_sge.addr = rdd_cl_req->temp_data;
        rdd_cl_req->data.data_sge.addr = rdd_cl_req->rdd_cl_cmd->cmd.hread.haddr;
        rdd_cl_req->data.data_sge.lkey = rdd_cl_req->data_mr->lkey;
        rdd_cl_req->data.data_wr.next = NULL;
        rdd_cl_req->data.data_sge.length = rdd_cl_req->rdd_cl_cmd->cmd.hread.len;

        //TODO: Send data read Request
        //rc = ibv_post_send(rdd_cl_req->q->qp, &rdd_cl_req->rsp.send_wr, &bad_wr);
        //fprintf(stdout, "Initate data transfer from %p to %p for %d size\n",
          //              rdd_cl_req->data.data_wr.wr.rdma.remote_addr,
            //            rdd_cl_req->data.data_sge.addr,
              //          rdd_cl_req->data.data_sge.length);

        rc = ibv_post_send(rdd_cl_req->q->qp, &rdd_cl_req->data.data_wr, NULL);
        assert(rc == 0);
        //assert(bad_wr == NULL);

        break;
    case RDD_CL_WR_TYPE_DATA_READ:
        rdd_cl_req = containerof(rdd_cl_wr, struct rdd_cl_req_s, data);

 //       fprintf(stdout, "Data transfer completed from %p to %p for %d size\n",
   //                     rdd_cl_req->data.data_wr.wr.rdma.remote_addr,
     //                   rdd_cl_req->data.data_sge.addr,
       //                 rdd_cl_req->data.data_sge.length);
        //TODO: Temp check data
        ibv_dereg_mr(rdd_cl_req->data_mr); //TODO: Verify performance impact
        //free(rdd_cl_req->temp_data); //TMP free data

        rc = ibv_post_send(rdd_cl_req->q->qp, &rdd_cl_req->rsp.send_wr, &bad_wr);
     //   fprintf(stdout, "Response Sent\n");
        assert(rc == 0);
        assert(bad_wr == NULL);

        break;
    case RDD_CL_WR_TYPE_RSP_SEND:
       // fprintf(stdout, "Response acknowledged\n");
        rdd_cl_req = containerof(rdd_cl_wr, struct rdd_cl_req_s, rsp);
        assert(rdd_cl_req->state == RDD_CL_REQ_RECV);
        rdd_cl_req->state = RDD_CL_REQ_FREE;
        ibv_post_recv(rdd_cl_req->q->qp, &rdd_cl_req->req.recv_wr, &bad_wr);
        assert(bad_wr == NULL);
        break;
    default:
        fprintf(stderr, "Unknown wc type recieved\n");
        break;
    }
    return 0;
}

void *poll_cq(void *arg)
{
    struct ibv_comp_channel *ch = (struct ibv_comp_channel *)arg;
    int rc;

    while(1) {
        struct ibv_cq *cq;
		struct ibv_wc wc;
		void *ctx;

        ibv_get_cq_event(ch, &cq, &ctx);
		ibv_ack_cq_events(cq, 1);
		ibv_req_notify_cq(cq, 0);

        while (ibv_poll_cq(cq, 1, &wc)) {
			rc = rdd_cl_process_wc(&wc);
            //TODO: Process error case
        }
    }
    
    return NULL;
}

int rdd_cl_cm_addr_resolved(struct rdma_cm_id *id)
{
	const char *name;
    int rc = 0;
	struct rdd_cl_dev_s *dev;
	struct ibv_device_attr_ex device_attr;
    rdd_cl_queue_t *queue = (rdd_cl_queue_t *)id->context;
	
    rdd_cl_conn_ctx_t *conn_ctx = queue->conn;

	fprintf(stdout, "qid %u # of completion vector: %d\n",
			queue->qid, id->verbs->num_comp_vectors);

	ibv_query_device_ex(id->verbs, NULL, &device_attr);
	if (device_attr.orig_attr.device_cap_flags & IBV_DEVICE_MEM_WINDOW ||
		device_attr.orig_attr.device_cap_flags & IBV_DEVICE_MEM_WINDOW_TYPE_2B) {
			printf("MW Supported\n");
	} else {
 			printf("MW Unupported\n");
	}

    pthread_mutex_lock(&conn_ctx->conn_lock);

	name = ibv_get_device_name(id->verbs->device);
	dev = rdd_cl_find_dev(conn_ctx->cl_ctx, name);
	if (dev == NULL) {
		fprintf(stderr, "No matching device %s\n", name);
		return -1;
	}

	//tgt->dev = ddev;//TODO: Find and assign target device
    conn_ctx->device = dev;
  	queue->ibv_ctx = id->verbs;

    if(conn_ctx->ch == NULL) {
        conn_ctx->ch = ibv_create_comp_channel(queue->cm_id->verbs);

        pthread_create(&conn_ctx->th.pt.cq_poll_thread, NULL, poll_cq, conn_ctx->ch);
    }

	stick_this_thread_to_core(conn_ctx->th.pt.cq_poll_thread, 30);
	pthread_setschedprio(conn_ctx->th.pt.cq_poll_thread, sched_get_priority_max(SCHED_OTHER));

    pthread_mutex_unlock(&conn_ctx->conn_lock);

	rc = rdd_cl_init_ib_queue(queue);
    assert(rc == 0);

	rc = rdma_resolve_route(queue->cm_id, RDD_CL_TIMEOUT_IN_MS);
    assert(rc == 0);

	return 0;
}

int rdd_cl_cm_route_resolved(struct rdma_cm_id *id)
{
	int ret;
    struct rdd_queue_priv_s priv;
	struct rdma_conn_param conn_param;
	struct ibv_device_attr device_attr;
    rdd_cl_queue_t *queue = (rdd_cl_queue_t *)id->context;
    rdd_cl_conn_ctx_t *conn = queue->conn;

	fprintf(stdout, "qid %u route resolved.\n", queue->qid);

    priv.data.client.proto_ver = RDD_PROTOCOL_VERSION;
    priv.data.client.qid = queue->qid;
    priv.data.client.hsqsize = conn->qd;
    priv.data.client.hrqsize = conn->qd;

	 memset(&conn_param, 0, sizeof(struct rdma_conn_param));

	 ret = ibv_query_device(id->verbs, &device_attr);
	 if (ret) {
	 	fprintf(stderr, "Failed to query device %s", id->verbs->device->name);
	 	return -1;
	 }

	conn_param.initiator_depth = device_attr.max_qp_init_rd_atom;
	conn_param.responder_resources = device_attr.max_qp_rd_atom;
	conn_param.retry_count = 7;
	conn_param.rnr_retry_count = 7;
	conn_param.flow_control = 1;
	conn_param.private_data = &priv;
	conn_param.private_data_len = sizeof (priv);

	 ret = rdma_connect(id, &conn_param);
	 if (ret) {
	 	fprintf(stderr, "Failed to connect to qpair %d\n", queue->qid);
	 	return -1;
	 }

	return 0;
}

uint16_t rdd_cl_conn_get_qhandle(void *arg)
{
	rdd_cl_conn_ctx_t *conn = (rdd_cl_conn_ctx_t *)arg;

	return conn->qhandle;
}

int rdd_cl_cm_conn_established(struct rdma_cm_event *ev) {
    struct rdma_cm_id *id = ev->id;
	rdd_cl_queue_t *q = (rdd_cl_queue_t *)id->context;
	rdd_cl_conn_ctx_t *conn = q->conn;
    struct rdd_queue_priv_s *priv = (struct rdd_queue_priv_s *) ev->param.conn.private_data;

    int i = 0, rc;
    struct ibv_recv_wr *bad_wr = NULL;

	fprintf(stdout, "qid %u connection established\n", q->qid);
    fprintf(stdout, "qid %u client handle %8x\n", q->qid, priv->data.server.qhandle);

	conn->qhandle = priv->data.server.qhandle;

    //TODO: Post commands
	//for (unsigned i=0; i<tgt->qdepth; i++)
	//	dr_host_post_recv(&q->rsp_ring[i]);
    for(i=0; i < conn->qd; i++) {
        rc = ibv_post_recv(q->qp, &q->reqs[i].req.recv_wr, &bad_wr);
        //TODO: Handle error code
        assert(bad_wr == NULL);
    }

	q->state = RDD_CL_Q_LIVE;

    //TODO: Increment live queue counter

	return 0;
}

int rdd_cl_on_disconnect(struct rdma_cm_id *id)
{
  printf("disconnected.\n");

  //TODO: tear down connection?? wait for all queues
  return 1; /* exit event loop */
}

static int rdd_cl_cm_event_handler(struct rdma_cm_event *ev)
{
	int r = 0;

	switch (ev->event) {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
            r = rdd_cl_cm_addr_resolved(ev->id);
            break;
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
            r = rdd_cl_cm_route_resolved(ev->id);
            break;
        case RDMA_CM_EVENT_ESTABLISHED:
            r = rdd_cl_cm_conn_established(ev);
            break;
        case RDMA_CM_EVENT_DISCONNECTED:
            r = rdd_cl_on_disconnect(ev->id);
            break;
        case RDMA_CM_EVENT_REJECTED:
            fprintf(stderr, "event status %d\n", ev->status);
            break;
        default:
            fprintf(stderr, "unsupported cm_event_handler: %d\n", ev->event);
            //die("on_event: unknown event.");
	}

	return r;
}

void *_rdd_cl_cm_event_task(void *arg)
{
    struct rdd_client_ctx_s *ctx = (struct rdd_client_ctx_s *)arg;
	struct rdma_event_channel *ch = ctx->cm_ch;
	struct rdma_cm_event *event;

	while (rdma_get_cm_event(ch, &event) == 0) {
        printf("Processing event %s\n", rdma_event_str(event->event));
		if (rdd_cl_cm_event_handler(event))
    		break;

		rdma_ack_cm_event(event);
        //TODO: Respond to cancellation request
  	}
    printf("Exiting cm event task %p\n", ctx);

	return NULL;
}

int rdd_cl_init_queues(rdd_cl_conn_ctx_t *ctx)
{
    int i, rc;
    rdd_cl_queue_t *queue;

    //TODO: validate upper limit for queuec
    ctx->queues = (rdd_cl_queue_t *) calloc(ctx->queuec, sizeof(rdd_cl_queue_t));
    if(!ctx->queues) {
        return -1;
    }

    for(i=0; i < ctx->queuec; i++) {
        queue = ctx->queues + i;
        queue->conn = ctx;
        rc = pthread_mutex_init(&queue->qlock, NULL);
        if(rc) {
            rdd_cl_destory_queues(ctx);
            return rc;
        }

        rc = rdma_create_id(ctx->cl_ctx->cm_ch, &queue->cm_id, NULL, RDMA_PS_TCP);
        if (rc) {
            fprintf(stderr, "failed to create cm_id");
            rdd_cl_destory_queues(ctx);
            return NULL;
        }

        queue->qid = i;
        queue->state = RDD_CL_Q_INIT;
        queue->cm_id->context = queue;

        //TODO: Check if need to be moved
        //queue->pd = ibv_alloc_pd(queue->cm_id->verbs);
        //assert(queue->pd != NULL);//TODO: Handle error case

        rc = rdma_resolve_addr(queue->cm_id, NULL, ctx->ai->ai_addr, RDD_CL_TIMEOUT_IN_MS);
        if(rc != 0) {
            assert(rc == -1);
            fprintf(stderr, "rdma resolve failed %d\n", errno);
            abort();
        }
        //TODO: Handle error
    }

    return 0;
}

void rdd_cl_destory_queues(rdd_cl_conn_ctx_t *ctx)
{
    int i;

    if(ctx->queues) {
        for (i=0; i<ctx->queuec; i++) {
            if(ctx->queues[i].cm_id) {
                rdma_destroy_id(ctx->queues[i].cm_id);
            }

            pthread_mutex_destroy(&ctx->queues[i].qlock);
            //Error code?
        }

        free(ctx->queues);
        ctx->queues = NULL;
    }
}

rdd_cl_conn_ctx_t *rdd_cl_create_conn(struct rdd_client_ctx_s *cl_ctx, rdd_cl_conn_params_t params)
{
    rdd_cl_conn_ctx_t *conn_ctx = NULL;
    int rc = 0;

    conn_ctx = (rdd_cl_conn_ctx_t *)calloc(1, sizeof(rdd_cl_conn_ctx_t));
    if(!conn_ctx) {
        fprintf(stderr, "Failed to allocate context\n");
        return NULL;
    }

    conn_ctx->cl_ctx = cl_ctx;
    pthread_mutex_init(&conn_ctx->conn_lock, NULL);
    //TODO: Handle failure

    rc = getaddrinfo(params.ip, params.port, NULL, &conn_ctx->ai);
    if (rc) {
        fprintf(stderr, "failed to parse target addr: %s\n", gai_strerror(rc));
        conn_ctx->ai = NULL; //Reset in case it is not defined
        rdd_cl_destroy_connection(conn_ctx);
        return NULL;
    }

    //TODO: Update from params
    conn_ctx->queuec = RDD_CL_MAX_DEFAULT_QUEUEC;
    conn_ctx->qd = RDD_CL_DEFAULT_SEND_WR_NUM;

    rc = rdd_cl_init_queues(conn_ctx);
    if(rc) {
        rdd_cl_destroy_connection(conn_ctx);
    }

    return conn_ctx;
}

void rdd_cl_destroy_connection(rdd_cl_conn_ctx_t *ctx)
{
    if(ctx) {
        if(ctx->queues) {
            rdd_cl_destory_queues(ctx);
        }

        if(ctx->ai) {
            freeaddrinfo(ctx->ai);
        }

        ctx->ai = NULL;

        pthread_mutex_destroy(&ctx->conn_lock);
        //Handle failure??

        free(ctx);

    }
}

struct rdd_client_ctx_s *rdd_cl_init(void)
{
    struct rdd_client_ctx_s *ctx;
    int rc = 0;
    int device_count = 0;
    int i;

    ctx = (struct rdd_client_ctx_s *) calloc(1, sizeof(struct rdd_client_ctx_s));
    if(!ctx) {
        fprintf(stderr, "Failed to allocate context\n");
        return NULL;
    }

    TAILQ_INIT(&ctx->devices);

    //TODO: lock this devices data structure in case need to be updated
    ctx->ibv_devs = ibv_get_device_list(&device_count);
    if(ctx->ibv_devs == NULL) {
        fprintf(stderr, "Failed to get rdma device list %d\n", errno);
        rdd_cl_destroy(ctx);
        return NULL;
    }

    for(i=0; i < device_count; i++) {
        rdd_cl_dev_t *dev = (rdd_cl_dev_t *)calloc(1, sizeof(rdd_cl_dev_t));
        assert(dev != NULL);//TODO: handle error
        dev->dev = ctx->ibv_devs[i];
        TAILQ_INSERT_TAIL(&ctx->devices, dev, dev_link);
    }

    ctx->cm_ch = rdma_create_event_channel();
    if(!ctx->cm_ch) {
        fprintf(stderr, "Failed to create rdma event channel %d\n", errno);
        rdd_cl_destroy(ctx);
        return NULL;
    }

    rc = pthread_create(&ctx->th.pt.cm_thread, NULL, _rdd_cl_cm_event_task, ctx);
    if(rc) {
        fprintf(stderr, "pthread create failed with error %d\n", rc);
        ctx->th.pt.cm_thread = NULL;//cm_thread is undefined for pthread create failure
        rdd_cl_destroy(ctx);
        return NULL;
    }

	stick_this_thread_to_core(ctx->th.pt.cm_thread, 25);

    return ctx;

}

void rdd_cl_destroy(struct rdd_client_ctx_s *ctx)
{
    int rc = 0;

    if(ctx->th.pt.cm_thread) {
        rc = pthread_cancel(ctx->th.pt.cm_thread);
        //TODO: validate error code

        rc = pthread_join(ctx->th.pt.cm_thread, NULL);
        //TODO: validate error code

        ctx->th.pt.cm_thread = NULL;
    }

    if(ctx->cm_ch) {
        rdma_destroy_event_channel(ctx->cm_ch);
    }

    if(ctx) {
        free(ctx);
    }
}
