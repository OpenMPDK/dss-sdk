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


#ifndef DRAGONFLY_QOS_H
#define DRAGONFLY_QOS_H


/**
 * \file
 * dragonfly WAL definitions
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "spdk/stdinc.h"
#include "spdk/assert.h"


#include "df_def.h"

struct dragonfly_qos {
	struct dragonfly_qos_ops qops; /**<services that this component provides */
	struct dragonfly_ops *dops; /**< expected services from framework */


};

struct dragonfly_qos_ops {
	int (*qos_queue_io)(struct dfly_request *req);
	int (*qos_io_completed)(struct dfly_request *req);

};

int qos_queue_io(struct dfly_request *req); /**< WAL inward API */

int qos_init(IN struct dragonfly_subsystem *pool, IN struct dragonfly_ops *dops,
	     IN OUT struct dragonfly_qos_ops *qops, int no_of_queues, int no_of_cores);
int qos_finish(IN struct dragonfly_subsystem *pool);

#ifdef __cplusplus
}
#endif

#endif // DRAGONFLY_QOS_H
