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

#include "spdk/stdinc.h"

#include "spdk/env.h"

#include "utils/dss_keygen.h"
#include "utils/dss_hsl.h"

static int g_num_items = 10;
static int g_key_size = 30;

static void test_dss_hsl_mem_usage(void)
{

	int i;
	char key[g_key_size];

	dss_hsl_ctx_t *ctx = dss_hsl_new_ctx("meta", "/", NULL);

	printf("****************Insertion***************\n");
	dss_keygen_init(g_key_size);

	for (i=0; i < g_num_items; i++) {
		dss_keygen_next_key(key);
		key[g_key_size] = '\0';

		if(i == 0) {printf("Sample key: %s\n", key);}

		dss_hsl_insert(ctx, key);
	}

	dss_hsl_print_info(ctx);

	printf("****************Deletion***************\n");
	dss_keygen_init(g_key_size);

	for (i=0; i < g_num_items; i++) {
		dss_keygen_next_key(key);
		key[g_key_size] = '\0';

		if(i == 0) {printf("Sample key: %s\n", key);}

		dss_hsl_delete(ctx, key);
	}

	dss_hsl_print_info(ctx);
}

int print_list_item(void *ctx, char *key, int is_file)
{
	if(is_file) {
		printf("%s\n", key);
	} else {
		printf("%s/\n", key);
	}

	return 0;

}

static void test_dss_hsl(void)
{
	dss_hsl_ctx_t *ctx = dss_hsl_new_ctx("meta", "/", print_list_item);

	assert(ctx);

	dss_hsl_insert(ctx, "meta/benixon/ben10");
	dss_hsl_insert(ctx, "sama/rah");
	dss_hsl_insert(ctx, "Arul/dhas");
	dss_hsl_insert(ctx, "meta/ben/plus/minus");
	dss_hsl_insert(ctx, "meta/ben/minus");
	dss_hsl_insert(ctx, "meta/Benixon Arul dhas");

	dss_hsl_list(ctx, "", NULL, NULL);
	dss_hsl_list(ctx, "meta", NULL, NULL);
	dss_hsl_list(ctx, "meta/benixon", NULL, NULL);
	dss_hsl_list(ctx, "meta/ben", NULL, NULL);
	dss_hsl_list(ctx, "meta/ben/plus", NULL, NULL);
	dss_hsl_list(ctx, "meta/ben/", "plus", NULL);
	dss_hsl_list(ctx, "meta/ben/", "minus", NULL);
	dss_hsl_list(ctx, "meta/ben/plus/", NULL, NULL);
	dss_hsl_list(ctx, "/meta/ben/plus/", NULL, NULL);
	dss_hsl_list(ctx, "/meta/ben/-1", NULL, NULL);

}

int main(int argc, char **argv)
{
	struct spdk_env_opts opts;

	spdk_env_opts_init(&opts);
	opts.name = "test_hsl";
	opts.shm_id = 0;
	if (spdk_env_init(&opts) < 0) {
		fprintf(stderr, "Unable to initialize SPDK env\n");
		return 1;
	}

	test_dss_hsl();

	if(argc > 1) {
		g_num_items = atoi(argv[1]);
		printf("Update num items to %d\n", g_num_items);
	}

	if(argc > 2) {
		g_key_size = atoi((argv[2]));
		printf("Update key size to %d\n", g_key_size);
	}

	//test_dss_hsl_mem_usage();

	return 0;
}
