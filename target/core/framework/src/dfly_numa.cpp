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


#include <dragonfly.h>
#include <nvmf_internal.h>

#define MAX_CPUS (1024)
#define DFLY_SOCKET_ID_ANY (-1)

//TODO: Better handling after allocation failure

typedef enum core_type_s {
	DFLY_NUMA_OTHER_CORE = 0,
	DFLY_NUMA_MASTER_CORE = 1,
} core_type_t;

typedef struct dfly_numa_info_s {
	unsigned socket_id;
	TAILQ_ENTRY(dfly_numa_info_s) link;
} dfly_numa_info_t;

dfly_numa_info_t g_numa_info_arr[MAX_CPUS];

typedef struct dfly_cpu_info_s {
	unsigned socket_id;
	uint32_t cpu_id;
	core_type_t type;
	int usage;//0 or 1
	TAILQ_ENTRY(dfly_cpu_info_s) link;
} dfly_cpu_info_t;

dfly_cpu_info_t g_cpu_info_arr[MAX_CPUS];

struct dfly_peer_alloc_s {
	char *peer_id;
	uint32_t current_usage;
	TAILQ_ENTRY(dfly_peer_alloc_s) peer_link;
	//NOTE: peer_cpu_usage should be last member of the struct
	uint32_t peer_cpu_usage[0];//Alloc max number of CPU in group
};

typedef struct dfly_cpu_group_context_s {
	pthread_mutex_t group_lock;
	int num_cpus;
	int max_usage;
	TAILQ_HEAD(, dfly_cpu_info_s) cpu_list;
	TAILQ_HEAD(, dfly_peer_alloc_s) peer_list;
} dfly_cpu_group_context_t;

typedef struct dfly_numa_group_list_item_s {
	char id_str[MAX_STRING_LEN + 1];
	dfly_cpu_group_context_t *group;
	unsigned socket;
	uint32_t ref_count;
	TAILQ_ENTRY(dfly_numa_group_list_item_s) link;
} dfly_numa_group_list_item_t;

struct dfly_numa_context {
	bool initialized;
	int total_cpus;
	int total_sockets;
	int avail_cpus;
	pthread_mutex_t ctx_lock;//Global lock
	dfly_cpu_info_t *master_core;
	TAILQ_HEAD(, dfly_cpu_info_s) cpu_list;
	TAILQ_HEAD(, dfly_numa_info_s) numa_list;
	TAILQ_HEAD(, dfly_numa_group_list_item_s) numa_group_list;
} g_dfly_numa_ctx;



//Helper functions
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

int _dfly_spdk_get_ifaddr_numa_node(char *if_addr);

static int
dfly_spdk_get_ifaddr_numa_node(char *if_addr)
{
	_dfly_spdk_get_ifaddr_numa_node(if_addr);
}
//Helper end

dfly_cpu_group_context_t *dfly_lookup_group_no_fail(unsigned socket_id);

void dfly_lookup_add_numa_node(unsigned socket_id)
{
	int i;
	for (i = 0; i < g_dfly_numa_ctx.total_sockets; i++) {
		if (g_numa_info_arr[i].socket_id == socket_id) {
			return;//Already added
		}
	}

	g_numa_info_arr[g_dfly_numa_ctx.total_sockets].socket_id = socket_id;

	TAILQ_INSERT_TAIL(&g_dfly_numa_ctx.numa_list,
			  &g_numa_info_arr[g_dfly_numa_ctx.total_sockets],
			  link);
	g_dfly_numa_ctx.total_sockets++;

	assert(g_dfly_numa_ctx.total_sockets <= MAX_CPUS);
	return;
}

//Caller should hold global ctx lock
dfly_cpu_info_t *dfly_allocate_next_cpu(unsigned socket_id)
{

	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;
	assert(socket_id != DFLY_SOCKET_ID_ANY);

	TAILQ_FOREACH_SAFE(cpu_info, &g_dfly_numa_ctx.cpu_list, link, tmp_cpu_info) {
		if (cpu_info->type == DFLY_NUMA_MASTER_CORE) {
			continue;
		}
		if (socket_id != DFLY_SOCKET_ID_ANY) {
			if (cpu_info->socket_id != socket_id) {
				continue;
			}
		}
		TAILQ_REMOVE(&g_dfly_numa_ctx.cpu_list, cpu_info, link);
		g_dfly_numa_ctx.avail_cpus--;

		assert(cpu_info);
		break;
	}
	return cpu_info;
}

//Caller should hold global ctx lock
dfly_numa_info_t *dfly_get_next_numa(dfly_numa_info_t *curr_numa)
{
	dfly_numa_info_t *numa = NULL;

	if (curr_numa) {
		numa = TAILQ_NEXT(curr_numa, link);
	} else {
		numa = TAILQ_FIRST(&g_dfly_numa_ctx.numa_list);
		assert(numa);
	}
	return numa;
}
//Caller should hold global ctx lock
dfly_cpu_group_context_t *dfly_allocate_cpu_group(unsigned socket_id, uint32_t num_cpus)
{
	dfly_cpu_group_context_t *gr_ctx = NULL;
	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;

	//For balanced socket alloc
	int socket_missed = 0;
	dfly_numa_info_t *next_numa = NULL;

	gr_ctx = (dfly_cpu_group_context_t *)calloc(1, sizeof(dfly_cpu_group_context_t));
	if (!gr_ctx) {
		return NULL;
	}

	pthread_mutex_init(&gr_ctx->group_lock, NULL);
	TAILQ_INIT(&gr_ctx->cpu_list);
	TAILQ_INIT(&gr_ctx->peer_list);

	while (1) {
		if (socket_id != DFLY_SOCKET_ID_ANY) {
			//Specific NUMA;
			cpu_info = dfly_allocate_next_cpu(socket_id);
			if (!cpu_info) {
				break;//While(1)
			}
		} else {
			//Balaced Allocation
			next_numa = dfly_get_next_numa(next_numa);
			if (next_numa == NULL) {
				//Finished a cycle check and reset
				if (socket_missed ==  g_dfly_numa_ctx.total_sockets) {
					//No more CPU available break
					break;
				} else {
					//Reset for next iteration
					socket_missed = 0;
					continue;
				}
			}
			cpu_info = dfly_allocate_next_cpu(next_numa->socket_id);
			if (!cpu_info) {
				socket_missed++;
				continue;
			}
		}

		TAILQ_INSERT_TAIL(&gr_ctx->cpu_list,
				  cpu_info,
				  link);

		gr_ctx->num_cpus++;

		num_cpus--;

		if (!num_cpus) {
			break;//While(1)
		}
	}

	TAILQ_FOREACH(cpu_info, &gr_ctx->cpu_list, link) {
		DFLY_INFOLOG(DFLY_LOG_NUMA, "Allocated Core %u in Socket %u\n", cpu_info->cpu_id,
			     cpu_info->socket_id);
	}

	if (num_cpus) {
		DFLY_WARNLOG("Unable to allocate %d cpus in socket %u\n", num_cpus, socket_id);
		if (!gr_ctx->num_cpus) {
			free(gr_ctx);
			gr_ctx = dfly_lookup_group_no_fail(socket_id);
		}
	}

	return gr_ctx;
}

//Caller should hold global ctx lock
void dfly_release_cpu_group(dfly_cpu_group_context_t *gr_ctx)
{
	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;

	pthread_mutex_lock(&gr_ctx->group_lock);//LOCK BEGIN

	assert(g_dfly_numa_ctx.initialized == true);

	TAILQ_FOREACH_SAFE(cpu_info, &gr_ctx->cpu_list, link, tmp_cpu_info) {
		TAILQ_REMOVE(&gr_ctx->cpu_list, cpu_info, link);
		TAILQ_INSERT_TAIL(&g_dfly_numa_ctx.cpu_list,
				  cpu_info,
				  link);

		g_dfly_numa_ctx.avail_cpus++;
	}
	pthread_mutex_unlock(&gr_ctx->group_lock);//LOCK END

	memset(gr_ctx, 0, sizeof(dfly_cpu_group_context_t));
	free(gr_ctx);

}

void _dfly_init_numa(void)
{
	uint32_t coreid;
	unsigned sock_id;

	int g_cpu_arr_index = 0;

	pthread_mutex_init(&g_dfly_numa_ctx.ctx_lock, NULL);

	pthread_mutex_lock(&g_dfly_numa_ctx.ctx_lock);//LOCK BEGIN

	TAILQ_INIT(&g_dfly_numa_ctx.cpu_list);
	TAILQ_INIT(&g_dfly_numa_ctx.numa_list);
	TAILQ_INIT(&g_dfly_numa_ctx.numa_group_list);
	g_dfly_numa_ctx.total_sockets = 0;

	SPDK_ENV_FOREACH_CORE(coreid) {
		sock_id = spdk_env_get_socket_id(coreid);

		assert(g_cpu_info_arr[g_cpu_arr_index].cpu_id  == 0);
		assert(g_cpu_info_arr[g_cpu_arr_index].socket_id == 0);

		g_cpu_info_arr[g_cpu_arr_index].cpu_id  = coreid;
		g_cpu_info_arr[g_cpu_arr_index].socket_id = sock_id;
		g_cpu_info_arr[g_cpu_arr_index].usage = 0;
		if (spdk_env_get_current_core() == coreid) {
			g_cpu_info_arr[g_cpu_arr_index].type = DFLY_NUMA_MASTER_CORE;
			assert(g_dfly_numa_ctx.master_core == NULL);
			g_dfly_numa_ctx.master_core = &g_cpu_info_arr[g_cpu_arr_index];
		} else {
			g_cpu_info_arr[g_cpu_arr_index].type = DFLY_NUMA_OTHER_CORE;
			dfly_lookup_add_numa_node(sock_id);
		}

		TAILQ_INSERT_TAIL(&g_dfly_numa_ctx.cpu_list,
				  &g_cpu_info_arr[g_cpu_arr_index],
				  link);

		g_cpu_arr_index++;
		if (g_cpu_arr_index >= MAX_CPUS) {
			assert(g_cpu_arr_index == MAX_CPUS);
			DFLY_WARNLOG("Using only %u CPUs\n", g_cpu_arr_index);
			break;
		}
	}

	g_dfly_numa_ctx.total_cpus = g_cpu_arr_index;
	g_dfly_numa_ctx.avail_cpus = g_cpu_arr_index;
	g_dfly_numa_ctx.initialized = true;

	assert(g_dfly_numa_ctx.master_core);

	pthread_mutex_unlock(&g_dfly_numa_ctx.ctx_lock);//LOCK END
#if 0
	dfly_cpu_info_t *cpu_info;
	TAILQ_FOREACH(cpu_info, &g_dfly_numa_ctx.cpu_list, link) {
		DFLY_WARNLOG("Core %u in Socket %u\n", cpu_info->cpu_id, cpu_info->socket_id);
	}

	//TEST Allocate
	DFLY_WARNLOG("Allocate socket 0\n");
	dfly_allocate_cpu_group(0, 5);
	DFLY_WARNLOG("Allocate socket 1\n");
	dfly_allocate_cpu_group(1, 5);
	DFLY_WARNLOG("Allocate socket any\n");
	dfly_allocate_cpu_group(-1, 15);
#endif
	return;
}

//Public APIs
void dfly_init_numa(void)
{
	_dfly_init_numa();

	return;
}

uint32_t dfly_get_master_core(void)
{
	assert(g_dfly_numa_ctx.initialized == true);
	return g_dfly_numa_ctx.master_core->cpu_id;
}

//Caller needs global lock
dfly_numa_group_list_item_s *dfly_lookup_group(char *name)
{
	struct dfly_numa_group_list_item_s *grp_info, *tmp_grp_info;

	TAILQ_FOREACH_SAFE(grp_info, &g_dfly_numa_ctx.numa_group_list, link, tmp_grp_info) {
		if (strncmp(grp_info->id_str, name, MAX_STRING_LEN)) {
			continue;
		}
		break;
	}

	return grp_info;

}

//Caller needs global lock
dfly_cpu_group_context_t *dfly_lookup_group_no_fail(unsigned socket_id)
{
	dfly_numa_group_list_item_s *grp_info   = NULL;
	dfly_numa_group_list_item_s *tmp_grp_info   = NULL;
	dfly_numa_group_list_item_s *iter_grp_info   = NULL;

	grp_info = TAILQ_FIRST(&g_dfly_numa_ctx.numa_group_list);
	assert(grp_info);

	TAILQ_FOREACH_SAFE(iter_grp_info, &g_dfly_numa_ctx.numa_group_list, link, tmp_grp_info) {
		if (iter_grp_info->socket == socket_id) {
			grp_info = iter_grp_info;
			break;
		}
	}

	assert(grp_info);
	return grp_info->group;
}

dfly_cpu_group_context_t *dfly_lookup_add_group(char *name, char *ip, int num_cpu)
{
	struct dfly_numa_group_list_item_s *grp_info, *tmp_grp_info;

	assert(g_dfly_numa_ctx.initialized == true);
	pthread_mutex_lock(&g_dfly_numa_ctx.ctx_lock);//LOCK BEGIN

	grp_info = dfly_lookup_group(name);

	if (!grp_info) {
		int numa_node = -1;

		grp_info = (struct dfly_numa_group_list_item_s *)calloc(1, sizeof(dfly_numa_group_list_item_t));
		if (!grp_info) {
			assert(0);
			pthread_mutex_unlock(&g_dfly_numa_ctx.ctx_lock);//LOCK RELEASE
			return NULL;
		}

		strncpy(grp_info->id_str, name, MAX_STRING_LEN);
        if(ip) {
		    numa_node = dfly_spdk_get_ifaddr_numa_node(ip);
        }
		if (numa_node == -1) {
			DFLY_INFOLOG(DFLY_LOG_NUMA, "No NUMA alignment for %s\n", grp_info->id_str);
		}
		grp_info->group  = dfly_allocate_cpu_group(numa_node, num_cpu);
		assert(grp_info->group);
		grp_info->socket = numa_node;
		TAILQ_INSERT_TAIL(&g_dfly_numa_ctx.numa_group_list,
				  grp_info,
				  link);
	}

	grp_info->ref_count++;

	pthread_mutex_unlock(&g_dfly_numa_ctx.ctx_lock);//LOCK END

	return grp_info->group;
}

dfly_cpu_info_t *dfly_get_next_available_cpu(dfly_cpu_group_context_t *cpu_group, char *peer_addr)
{
	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;

	int i, j;
	bool inc_peer_cpu_usage = true;

	if(peer_addr) {
		struct dfly_peer_alloc_s *peer_grp, *tmp_peer_grp;
		uint32_t dest_cpu_index = 0, tmp_cpu_index;
		DFLY_ASSERT(strlen(peer_addr) != 0);
		TAILQ_FOREACH_SAFE(peer_grp, &cpu_group->peer_list, peer_link, tmp_peer_grp) {
			if(strncmp(peer_grp->peer_id, peer_addr, INET6_ADDRSTRLEN)) {
				continue;//Mismatch
			} else {
				break;//Found
			}
		}

		if(!peer_grp) {
			//Alloc new
			peer_grp = (struct dfly_peer_alloc_s *)calloc(1, sizeof(struct dfly_peer_alloc_s) + \
								(sizeof(uint32_t) * cpu_group->num_cpus));
			DFLY_ASSERT(peer_grp);

			peer_grp->peer_id = strndup(peer_addr, INET6_ADDRSTRLEN);
			TAILQ_INSERT_TAIL(&cpu_group->peer_list, peer_grp, peer_link);
		}

		//Select from peer group
		for(i=0; i < cpu_group->num_cpus; i++) {
			if(peer_grp->peer_cpu_usage[i] <= peer_grp->current_usage) {
				DFLY_ASSERT(peer_grp->peer_cpu_usage[i] == peer_grp->current_usage);
				peer_grp->peer_cpu_usage[i]++;
				dest_cpu_index = i;
				for(j=i+1; j < cpu_group->num_cpus; j++) {
					if(peer_grp->peer_cpu_usage[j] == peer_grp->current_usage) {
						inc_peer_cpu_usage = false;
						break;
					}
				}
				if(inc_peer_cpu_usage == true) {
					cpu_group->max_usage++;
					peer_grp->current_usage++;
				}
				break;
			}
			DFLY_ASSERT(peer_grp->peer_cpu_usage[i] > peer_grp->current_usage);
		}

		if(i == cpu_group->num_cpus) {
			cpu_group->max_usage++;
			peer_grp->current_usage++;
			peer_grp->peer_cpu_usage[0]++;
			dest_cpu_index = 0;
		}

		tmp_cpu_index = 0;
		TAILQ_FOREACH_SAFE(cpu_info, &cpu_group->cpu_list, link, tmp_cpu_info) {
			if (tmp_cpu_index == dest_cpu_index) {
				break;
			}
			tmp_cpu_index++;
		}
		DFLY_ASSERT(cpu_info);
		cpu_info->usage++;
	} else {
		DFLY_ASSERT(TAILQ_EMPTY(&cpu_group->peer_list));
		TAILQ_FOREACH_SAFE(cpu_info, &cpu_group->cpu_list, link, tmp_cpu_info) {
			if (cpu_info->usage <= cpu_group->max_usage) {
				break;
			}
		}

		if (!cpu_info) {
			cpu_info = TAILQ_FIRST(&cpu_group->cpu_list);
			cpu_group->max_usage++;
		}

		assert(cpu_info);
		cpu_info->usage++;
	}

	return cpu_info;
}

uint32_t dfly_get_next_conn_core(char *conn, int num_cpu, char *ip, char *peer_addr)
{
	dfly_cpu_group_context_t *cpu_group;

	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;

	assert(g_dfly_numa_ctx.initialized == true);

	cpu_group = dfly_lookup_add_group(conn, ip, num_cpu);
	assert(cpu_group);

	pthread_mutex_lock(&cpu_group->group_lock);//LOCK BEGIN

	//get_next_availble_cpu
	cpu_info = dfly_get_next_available_cpu(cpu_group, peer_addr);
	assert(cpu_info);

	DFLY_INFOLOG(DFLY_LOG_NUMA, "Allocating Core %d for conn %s for peer %s\n", cpu_info->cpu_id, conn, peer_addr?peer_addr:"not provided");
	pthread_mutex_unlock(&cpu_group->group_lock);//LOCK END

	return cpu_info->cpu_id;
}

void dfly_update_conn_usage(dfly_cpu_group_context_t *cpu_group, uint32_t core, char *peer_addr)
{
	int max_usage = 0;
	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;
	bool updated_for_core = false;

	TAILQ_FOREACH_SAFE(cpu_info, &cpu_group->cpu_list, link, tmp_cpu_info) {
		if (cpu_info->cpu_id == core) {
			cpu_info->usage--;
			updated_for_core = true;
		}
		if (cpu_info->usage > max_usage) {
			max_usage = cpu_info->usage;
		}
	}

	if(peer_addr) {
		int cpu_index = 0;
		struct dfly_peer_alloc_s *peer_grp, *tmp_peer_grp;

		DFLY_ASSERT(strlen(peer_addr) != 0);
		TAILQ_FOREACH_SAFE(cpu_info, &cpu_group->cpu_list, link, tmp_cpu_info) {
			if(cpu_info->cpu_id == core) {
				break;
			}
			cpu_index++;
		}
		DFLY_ASSERT(cpu_index < cpu_group->num_cpus);

		TAILQ_FOREACH_SAFE(peer_grp, &cpu_group->peer_list, peer_link, tmp_peer_grp) {
			if(strncmp(peer_grp->peer_id, peer_addr, INET6_ADDRSTRLEN)) {
				continue;//Mismatch
			} else {
				break;//Found
			}
		}

		DFLY_ASSERT(peer_grp);

		peer_grp->peer_cpu_usage[cpu_index]--;
		if(peer_grp->peer_cpu_usage[cpu_index] < peer_grp->current_usage) {
			DFLY_ASSERT(peer_grp->current_usage > 0);
			DFLY_ASSERT(cpu_group->max_usage > 0);
			peer_grp->current_usage--;
			cpu_group->max_usage--;
		}
		//TODO: Free peer link if all connections are closed?
	} else {
		DFLY_ASSERT(TAILQ_EMPTY(&cpu_group->peer_list));
		if (max_usage < cpu_group->max_usage) {
			cpu_group->max_usage--;
			assert(max_usage == cpu_group->max_usage);
		}
	}

	assert(updated_for_core == true);
	return;
}

uint32_t dfly_put_conn_core(char *conn, uint32_t core, char *peer_addr)
{
	struct dfly_numa_group_list_item_s *grp_info, *tmp_grp_info;
	dfly_cpu_info_t *cpu_info = NULL;
	dfly_cpu_info_t *tmp_cpu_info = NULL;

	assert(g_dfly_numa_ctx.initialized == true);
	pthread_mutex_lock(&g_dfly_numa_ctx.ctx_lock);//LOCK BEGIN

	grp_info = dfly_lookup_group(conn);
	assert(grp_info);

	pthread_mutex_lock(&grp_info->group->group_lock);//LOCK BEGIN

	DFLY_INFOLOG(DFLY_LOG_NUMA, "De-Allocating Core %d on conn %s for peer %s\n", core, conn, peer_addr?peer_addr:"not provided");
	//Lookup and update usage max count
	dfly_update_conn_usage(grp_info->group, core, peer_addr);

	pthread_mutex_unlock(&grp_info->group->group_lock);//LOCK END

	pthread_mutex_unlock(&g_dfly_numa_ctx.ctx_lock);//LOCK END

}

uint32_t dfly_get_next_core(char *conn, int num_cpu, char *ip, char *peer_addr)
{
	assert(g_dfly_numa_ctx.initialized == true);

	return dfly_get_next_conn_core(conn, num_cpu, ip, peer_addr);
}

uint32_t dfly_put_core(char *conn, int core, char *peer_addr)
{
	assert(g_dfly_numa_ctx.initialized == true);

	return dfly_put_conn_core(conn, core, peer_addr);
}
