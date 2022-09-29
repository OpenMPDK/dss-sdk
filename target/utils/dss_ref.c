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

//Util Wrappers

#include "spdk/env.h"
#include "dragonfly.h"
#include "utils/dss_ref.h"

uint32_t dss_ref_get_max_idx(void)
{
    return spdk_env_get_last_core();
}

uint32_t dss_ref_get_curr_idx(void)
{
    return spdk_env_get_current_core();
}
//End - Util Wrappers

struct dss_ref_s *dss_ref_create(void)
{
    struct dss_ref_s * r = NULL;
    uint32_t rarr_count = dss_ref_get_max_idx() + 1;

    r = calloc(1, sizeof(struct dss_ref_s) + (rarr_count * sizeof(uint32_t)));
    if(r) {
        r->rarr_cnt = rarr_count;
        r->frozen = 0;
    }

    return r;
}

void dss_ref_destroy(struct dss_ref_s *r)
{
    DSS_ASSERT(r);
    free(r);
}

void dss_ref_freeze(struct dss_ref_s *r)
{
    __atomic_fetch_add(&r->frozen, 1, __ATOMIC_RELAXED);
}

bool dss_ref_inc(struct dss_ref_s *r)
{
    uint32_t index = dss_ref_get_curr_idx();
    int frozen;

    frozen = __atomic_load_n(&r->frozen, __ATOMIC_RELAXED);
    if(frozen) { //unlikely
        return false;
    }
    DSS_ASSERT(index < r->rarr_cnt);
    r->ref_arr[index]++;

    return true;
}

//Call only if dss_ref_inc succeeds
void dss_ref_dec(struct dss_ref_s *r)
{
    uint32_t index = dss_ref_get_curr_idx();

    DSS_ASSERT(index <= r->rarr_cnt);
    DSS_ASSERT(r->ref_arr[index] != 0);
    r->ref_arr[index]--;
}

bool dss_ref_check(struct dss_ref_s *r) {
    bool in_use = false;
    int i;
    for(i = 0; i < r->rarr_cnt; i++) {
        if(r->ref_arr[i]) {
            in_use = true;
            break;
        }
    }
}
