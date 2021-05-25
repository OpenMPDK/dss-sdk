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

#include <Judy.h>
#include <string.h>
#include <malloc.h>

#include "dragonfly.h"
#include "rocksdb/dss_kv2blk_c.h"

#define DSS_LIST_MAX_KLEN (1024)

dss_hsl_ctx_t *dss_hsl_new_ctx(char *root_prefix, char *delim_str, list_item_cb list_cb)
{
	dss_hsl_ctx_t *hsl_ctx = (dss_hsl_ctx_t *)calloc(1, sizeof(dss_hsl_ctx_t));

	if(hsl_ctx) {
		hsl_ctx->root_prefix = strdup(root_prefix);
		hsl_ctx->delim_str = strdup(delim_str);

		hsl_ctx->lnode.leaf = 0;
		assert(list_cb);
		hsl_ctx->process_listing_item = list_cb;
		hsl_ctx->lnode.subtree = NULL;
		TAILQ_INIT(&hsl_ctx->lru_list);
		hsl_ctx->mem_usage = 0;
		hsl_ctx->mem_limit = g_dragonfly->dss_judy_listing_cache_limit_size * 1024 * 1024;

		return hsl_ctx;
	} else {
		return NULL;
	}
}

void _dss_hsl_delete_subtree(dss_hsl_ctx_t *hctx, dss_hslist_node_t *tnode)
{
	uint8_t key_str[DSS_LIST_MAX_KLEN + 1];
	Word_t * value;
	dss_hslist_node_t *node;
	int rc;

	if(tnode->leaf) {
		assert(tnode->subtree == NULL);
		//Called to delete leaf_node directly assert
		assert(0);
	}

	strcpy(key_str, "");
	value = (Word_t *) JudySLFirst(tnode->subtree, key_str, PJE0);

	//DFLY_NOTICELOG("Sample delete str [%s]\n", key_str);

	while(value) {
		node = *value;
		if(node->leaf == 1) {
			//Delete and return
			assert(node->subtree == NULL);
			//printf("Delete Leaf [%s]\n", key_str);
			rc = JudySLDel(&tnode->subtree, key_str, PJE0);
			assert(rc == 1);

			//Remove only non-leaf
			//Free the origial node not parent tnode
			//TAILQ_REMOVE(&hctx->lru_list, node, lru_link);

			//Update memory usage on delete
			hctx->mem_usage -= sizeof(dss_hslist_node_t);
			hctx->mem_usage -= strlen(key_str);

			free(node);

			hctx->node_count--;
		} else {
			//Recurse the tree to delete subtree
			assert(node->subtree);
			_dss_hsl_delete_subtree(hctx, node);

			//printf("Delete non Leaf [%s]\n", key_str);
			JudySLFreeArray(&node->subtree, PJE0);
			node->subtree = NULL;

			//Remove from LRU list
			TAILQ_REMOVE(&hctx->lru_list, node, lru_link);

			//Update memory usage on delete
			hctx->mem_usage -= sizeof(dss_hslist_node_t);
			hctx->mem_usage -= strlen(key_str);

			free(node);

			hctx->node_count--;

			//Not deleting the root itself
		}
		value = (Word_t *) JudySLNext(tnode->subtree, key_str, PJE0);
	}

	tnode->list_direct = 1;
	return;
}

void dss_hsl_evict_cache_threshold(dss_hsl_ctx_t *hctx)
{
	dss_hslist_node_t *node;

	//DFLY_NOTICELOG("Before evict %ld/%ld \n", hctx->mem_usage, hctx->mem_limit);
	//TODO: make this percentage upper and lower??
	while(hctx->mem_usage > hctx->mem_limit) {

		assert(node != &hctx->lnode);

		node = TAILQ_LAST(&hctx->lru_list, lru_list_head);

		//TODO: Check progress
		if(node) {
			//Delete node subtree
			_dss_hsl_delete_subtree(hctx, node);
		} else {
			//Empty Cache??
			break;
		}
	}
	//DFLY_NOTICELOG("After  evict %ld/%ld \n", hctx->mem_usage, hctx->mem_limit);
}

void dss_hsl_evict_levels(dss_hsl_ctx_t *hctx, int num_evict_levels, dss_hslist_node_t *node, int curr_level)
{
	int num_rem_levels;

	Word_t *value;
	uint8_t list_str[DSS_LIST_MAX_KLEN + 1];

	assert(num_evict_levels >= 1);

	if(hctx->tree_depth <= num_evict_levels) {
		return;
	}

	assert(curr_level <= hctx->tree_depth);
	num_rem_levels = hctx->tree_depth - curr_level + 1;

	if (node->leaf) {
		return; //Short keys
	} else if(num_rem_levels == num_evict_levels) {
		_dss_hsl_delete_subtree(hctx, node);
	} else {
		assert(num_rem_levels > num_evict_levels);
		//Traverse tree
		strcpy(list_str, "");

		value = (Word_t *) JudySLFirst(node->subtree, list_str, PJE0);

		while(value) {
			dss_hslist_node_t *next_node = (dss_hslist_node_t *)*value;

			//TODO: It's possible to encounter list_direct nodes if multiple evicts are run
			assert(next_node->list_direct == 0);

			//printf("Try evict for level %d for str [%s]\n", curr_level, list_str);
			dss_hsl_evict_levels(hctx, num_evict_levels, next_node, curr_level + 1);

			value = (Word_t *) JudySLNext(node->subtree, list_str, PJE0);
		}
	}

}

int _dss_hsl_delete_key(dss_hsl_ctx_t *hctx, dss_hslist_node_t *tnode, char *tok, char **saveptr)
{

	char *token_next;

	Word_t *value;
	int rc;

	if(tnode->list_direct == 1) {
		//printf("list direct skip token [%s]\n", tok);
		assert(tnode->leaf == 0);
		return 0;
	}

	token_next = strtok_r(NULL, hctx->delim_str, saveptr);
	if(token_next) {
		dss_hslist_node_t *next_node;
		uint8_t key_str[DSS_LIST_MAX_KLEN + 1];

		//hctx->node_count--;
		//Find next node and call recursive delete
		value = (Word_t *) JudySLGet(tnode->subtree, tok, PJE0);//Get token node for next level

		if(value) {
			next_node = (dss_hslist_node_t *)*value;
			rc = _dss_hsl_delete_key(hctx, next_node, token_next, saveptr);
			if(rc == 0) {
				return 0;
			}

			strcpy(key_str, "");
			value = (Word_t *) JudySLNext(next_node->subtree, key_str, PJE0);
			if(value) {
				//Judy SL array not empty
				return 0;
			} else {
				JudySLFreeArray(&next_node->subtree, PJE0);
				next_node->subtree = NULL;

				rc = JudySLDel(&tnode->subtree, tok, PJE0);
				assert(rc == 1);//Found Key should be deleted

				//Remove only non-leaf
				//Remove from LRU list
				if(next_node->leaf != 1) {
					TAILQ_REMOVE(&hctx->lru_list, next_node, lru_link);
				}

				//Update memory usage on delete
				hctx->mem_usage -= sizeof(dss_hslist_node_t);
				hctx->mem_usage -= strlen(tok);

				free(next_node);

				hctx->node_count--;

				return 1;
			}

		} else {
			//Key not found
			return 0;
		}
	} else {
		dss_hslist_node_t *leaf_node;

		//Check leaf and delete entry
		value = (Word_t *) JudySLGet(tnode->subtree, tok, PJE0);

		if(value) {
			leaf_node = (dss_hslist_node_t *)*value;

			rc = JudySLDel(&tnode->subtree, tok, PJE0);
			assert(rc == 1);//Found Key should be deleted

			assert(leaf_node->leaf == 1);
			assert(leaf_node->subtree == NULL);

			//Remove only non-leaf
			//Remove from LRU list
			//TAILQ_REMOVE(&hctx->lru_list, leaf_node, lru_link);

			//Update memory usage on delete
			hctx->mem_usage -= sizeof(dss_hslist_node_t);
			hctx->mem_usage -= strlen(tok);

			free(leaf_node);

			hctx->node_count--;

			return 1;
		} else {
			//Key not found
			return 0;
		}

		return 0;
	}

}

int dss_hsl_delete(dss_hsl_ctx_t *hctx, const char *key)
{
	Word_t *value;
	uint8_t key_str[DSS_LIST_MAX_KLEN + 1];

	char *tok, *saveptr;

	dss_hslist_node_t *tnode;
	uint32_t depth = 0;

	if(strlen(key) > DSS_LIST_MAX_KLEN) return -1;

	strncpy((char *)key_str, key, DSS_LIST_MAX_KLEN);
	key_str[DSS_LIST_MAX_KLEN] = '\0';

	tnode = &hctx->lnode;
	//Fist delmiter
	tok =  strtok_r(key_str, hctx->delim_str, &saveptr);

	if(tok && !strcmp(hctx->root_prefix, tok)) {
		//Advance token - root prefix in not saved
		tok = strtok_r(NULL, hctx->delim_str, &saveptr);
	} else {
		return 0;
	}

	return _dss_hsl_delete_key(hctx, tnode, tok, &saveptr);
}

int dss_hsl_insert(dss_hsl_ctx_t *hctx, const char *key)
{
	Word_t *value;
	uint8_t key_str[DSS_LIST_MAX_KLEN + 1];

	char *tok, *saveptr;

	dss_hslist_node_t *tnode;
	uint32_t depth = 0;
	int new_node;

	if(strlen(key) > DSS_LIST_MAX_KLEN) return -1;

	strncpy((char *)key_str, key, DSS_LIST_MAX_KLEN);
	key_str[DSS_LIST_MAX_KLEN] = '\0';

	//printf("Insert key [%s]\n", key_str);

	tnode = &hctx->lnode;
	//Fist delmiter
	tok =  strtok_r(key_str, hctx->delim_str, &saveptr);

	if(tok && !strcmp(hctx->root_prefix, tok)) {
		//Advance token - root prefix in not saved
		tok = strtok_r(NULL, hctx->delim_str, &saveptr);
	} else {
		return 0;
	}

	//Evict if more than limit
	if(hctx->mem_usage > hctx->mem_limit) {
		dss_hsl_evict_cache_threshold(hctx);
	}

	while(tok) {

		//printf("Insert token %s at node %p depth %d isleaf:%d \n", tok, tnode, depth, tnode->leaf);
		if(tnode->list_direct) {
			//printf("list direct skip token [%s]\n", tok);
			return 0;
		}

		value = (Word_t *)JudySLIns(&tnode->subtree, tok, PJE0);

		if(!*value) {
#if defined DSS_LIST_DEBUG_MEM_USE
			hctx->node_count++;
#endif
			*value = (Word_t) calloc(1, sizeof(dss_hslist_node_t));

			tnode = (dss_hslist_node_t *)*value;
			new_node = 1;
			//Update memory usage on new insert
			hctx->mem_usage += sizeof(dss_hslist_node_t);
			hctx->mem_usage += strlen(tok);


		} else {
			tnode = (dss_hslist_node_t *)*value;
			new_node = 0;
		}


		tok = strtok_r(NULL, hctx->delim_str, &saveptr);

		if(tok) {
		//if(tok || key[strlen(key) - 1] == hctx->delim) {
			tnode->leaf = 0;
			if(new_node == 1) {
				//Insert only Non-Leaf
				TAILQ_INSERT_HEAD(&hctx->lru_list, tnode, lru_link);
			} else {
				//Re-Insert to head only Non-Leaf
				TAILQ_REMOVE(&hctx->lru_list, tnode, lru_link);
				TAILQ_INSERT_HEAD(&hctx->lru_list, tnode, lru_link);
			}
		} else {
			tnode->leaf = 1;
		}

		if(tok) depth++;//Root node is depth 0
	}

	if(hctx->tree_depth < depth) {
		hctx->tree_depth = depth;
	}

	return 0;
}

Word_t dss_hsl_list_all(dss_hslist_node_t *node);

dss_hsl_print_info(dss_hsl_ctx_t *hctx)
{
	int count = 0;
	dss_hslist_node_t *node = NULL;

	//printf("JudySL mem usage word count %ld\n", JudyMallocMemUsed());
	printf("DSS hsl node count %ld\n", hctx->node_count);
	printf("DSS hsl tree dept %ld\n", hctx->tree_depth);
	printf("DSS hsl tree elment count  %ld\n", dss_hsl_list_all((dss_hslist_node_t *)&hctx->lnode));

	TAILQ_FOREACH(node, &hctx->lru_list, lru_link) {
		count++;
	}
	printf("DSS hsl lru list count %ld\n", count);
	printf("DSS hsl mem usage %ld\n", hctx->mem_usage);

}


Word_t dss_hsl_list_all(dss_hslist_node_t *node)
{
	Word_t *value;
	uint64_t leaf_count = 0;
	uint8_t list_str[DSS_LIST_MAX_KLEN + 1];

	if (node->leaf == 1) {
		return 1;
	}

	strcpy(list_str, "");

	value = (Word_t *) JudySLFirst(node->subtree, list_str, PJE0);

	while(value) {
		leaf_count += dss_hsl_list_all((dss_hslist_node_t *)*value);

		value = (Word_t *) JudySLNext(node->subtree, list_str, PJE0);
	}

	return leaf_count;
}

int dss_hsl_list(dss_hsl_ctx_t *hctx, const char *prefix, const char *start_key, void *listing_ctx)
{
	Word_t *value;
	uint8_t prefix_str[DSS_LIST_MAX_KLEN + 1];
	uint8_t list_str[DSS_LIST_MAX_KLEN + 1];

	char *tok, *saveptr;

	dss_hslist_node_t *tnode, *lnode;

	int rc = DFLY_LIST_READ_DONE;

	if(strlen(prefix) > DSS_LIST_MAX_KLEN) return rc;

	strncpy((char *)prefix_str, prefix, DSS_LIST_MAX_KLEN);
	prefix_str[DSS_LIST_MAX_KLEN] = '\0';

	tnode = &hctx->lnode;
	//Fist delmiter
	tok =  strtok_r(prefix_str, hctx->delim_str, &saveptr);

	if(tok && !strcmp(hctx->root_prefix, tok)) {
		//Advance token - root prefix in not saved
		tok = strtok_r(NULL, hctx->delim_str, &saveptr);
	}

	//printf("Prefix: %s\n", prefix);
	//Find Node
	while(tok) {
		value = (Word_t *) JudySLGet(tnode->subtree, tok, PJE0);

		if(!value) {
			//Entry not found
			printf("Coundn't find token %s\n", tok);
			return rc;
		}

		tnode = (dss_hslist_node_t *)*value;
		assert(tnode != NULL);

		//Re-Insert only Non-Leaf
		if(tnode->leaf != 1) {
			//Update LRU - move to head of List
			TAILQ_REMOVE(&hctx->lru_list, tnode, lru_link);
			TAILQ_INSERT_HEAD(&hctx->lru_list, tnode, lru_link);
		}

		if(tnode->list_direct) {
			struct dss_list_read_process_ctx_s *lctx = (struct dss_list_read_process_ctx_s *)listing_ctx;
			//TODO: Make call async to tpool
			//dss_rocksdb_direct_iter(hctx, prefix, start_key, listing_ctx);
			DFLY_ASSERT(lctx->parent_req);
			if(g_dragonfly->rdb_direct_listing_enable_tpool) {
				dss_tpool_post_request(hctx->dlist_mod, lctx->parent_req);
				rc = DFLY_LIST_READ_PENDING;
			} else {
				dss_rocksdb_direct_iter(hctx, lctx->parent_req);
				rc = DFLY_LIST_READ_DONE;
			}
			return rc;
		}

		tok = strtok_r(NULL, hctx->delim_str, &saveptr);

		if(!tok) {
			if(tnode->leaf == 1) {
				//printf("Not listing leaf str %s\n", prefix);
				//tnode = lnode;
				assert(0);
			}//Non leaf use node to list hierarchical entry
		}
	}

	strcpy(list_str, "");

	value = (Word_t *) JudySLFirst(tnode->subtree, list_str, PJE0);

	if(value && start_key) {
		strcpy(list_str, start_key);
		value = (Word_t *) JudySLNext(tnode->subtree, list_str, PJE0);
	}

	while (value) {
		assert(*value);
		lnode = (dss_hslist_node_t *)*value;

		if(lnode->leaf) {
			rc = hctx->process_listing_item(listing_ctx, list_str, 1);
		} else {
			rc = hctx->process_listing_item(listing_ctx, list_str, 0);
		}

		if(rc != 0) {
			return DFLY_LIST_READ_DONE;
		}

		value = (Word_t *) JudySLNext(tnode->subtree, list_str, PJE0);
	}

	return DFLY_LIST_READ_DONE;//READ DONE

}
