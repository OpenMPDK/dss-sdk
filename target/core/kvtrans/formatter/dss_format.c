#include "spdk/stdinc.h"

#include "spdk/bdev.h"
#include "spdk/event.h"
#include "spdk/string.h"

#include "string.h"

//#include "formatter.h"
#include "formatter_interface.h"


#define DSS_FORMATTER_DEFAULT_BLK_SIZE (0x1000)
#define DSS_FORMATTER_DEFAULT_NUM_BLK_STATES (6)
#define DSS_FORMATTER_DEFAULT_BLK_ALLOCATOR_TYPE "block_impresario"
#define DSS_FORMATTER_DEFAULT_DEBUG_OPTION (true)

enum dss_formatter_opts {
    DSS_FORMATTER_OPTION_BLK_SIZE = 0x1000, //Start with a high value so it does not overlap with spdk opts
    DSS_FORMATTER_OPTION_NUM_BLK_STATES,
    DSS_FORMATTER_OPTION_BLK_ALLOCATOR_TYPE,
    DSS_FORMATTER_OPTION_NO_VERIFY,
    DSS_FORMATTER_OPTION_DEV_NAME,
};

static struct option g_cmdline_opts[] = {
    {
        .name = "block_size",
        .has_arg = 1,
        .flag = NULL,
        .val = DSS_FORMATTER_OPTION_BLK_SIZE,
    },
    {
        .name = "num_block_states",
        .has_arg = 1,
        .flag = NULL,
        .val = DSS_FORMATTER_OPTION_NUM_BLK_STATES,
    },
    {
        .name = "block_allocator_type",
        .has_arg = 1,
        .flag = NULL,
        .val = DSS_FORMATTER_OPTION_BLK_ALLOCATOR_TYPE,
    },
    {
        .name = "no_verify",
        .has_arg = 0,
        .flag = NULL,
        .val = DSS_FORMATTER_OPTION_NO_VERIFY,
    },
    {
        .name = "dev_name",
        .has_arg = 1,
        .flag = NULL,
        .val = DSS_FORMATTER_OPTION_DEV_NAME,
    },
    {
        .name =""
    }
};

dss_formatter_config_opts_t g_dss_formatter_opts;

static void dss_formatter_set_default_opts(dss_formatter_config_opts_t *opts)
{
    opts->blk_size = DSS_FORMATTER_DEFAULT_BLK_SIZE;
    opts->nblk_states = DSS_FORMATTER_DEFAULT_NUM_BLK_STATES;
    opts->ba_type = DSS_FORMATTER_DEFAULT_BLK_ALLOCATOR_TYPE;
    opts->debug = DSS_FORMATTER_DEFAULT_DEBUG_OPTION;
    opts->dev_name = NULL;//Device name should be provided by user

    return;
}

//Ideally this should be the drive block size but 512 is a fairly good assumption for block drives
#define BLOCK_SMALLEST_SIZE_BYTES (512)

static bool dss_formatter_validate_config(dss_formatter_config_opts_t *opts)
{
    bool rc = true;

    if(opts->blk_size % BLOCK_SMALLEST_SIZE_BYTES) {
        printf("Block size provided must be a multiple of 512 bytes\n");
        rc = false;
    }

    if(opts->dev_name == NULL) {
        printf("Valid device name required\n");
        rc = false;
    }

    return rc;
}

static void usage (void) {
    printf("-------------DSS Formatter Usage------------\n");
    printf(" --dev_name <device name>. Required option, specify device file name configured. Usually 'n1' needs to be appended to spdk device name.\n");
    printf(" --block_size <block size>. Optional, Defaults to %d\n", DSS_FORMATTER_DEFAULT_BLK_SIZE);
    printf(" --num_block_states <total block states>. Optional, Defaults to %d\n",DSS_FORMATTER_DEFAULT_NUM_BLK_STATES); 
    printf(" --block_allocator_type <type string>. Optional, defaults to '%s'\n", DSS_FORMATTER_DEFAULT_BLK_ALLOCATOR_TYPE);
    printf(" --no_verify. Optional, verifies formatted info on disk by default\n");

    return;
}

static int dss_formatter_parse_args(int ch, char *optarg)
{
    long int tmp_option;
    switch(ch) {
        case DSS_FORMATTER_OPTION_BLK_SIZE:
            tmp_option = spdk_strtol(optarg, 10);
            if(tmp_option < 0) {
                printf("Invalid block size provided\n");
                usage();
                return 1;
            }
            g_dss_formatter_opts.blk_size = tmp_option;
            break;
        case DSS_FORMATTER_OPTION_NUM_BLK_STATES:
            //TODO: Number of block states needs to be calculated internally
            tmp_option = spdk_strtol(optarg, 10);
            if(tmp_option < 0) {
                printf("Invalid number of block states provided\n");
                usage();
                return 1;
            }
            g_dss_formatter_opts.nblk_states = tmp_option;
            break;
        case DSS_FORMATTER_OPTION_BLK_ALLOCATOR_TYPE:
            g_dss_formatter_opts.ba_type = strdup(optarg);
            break;
        case DSS_FORMATTER_OPTION_NO_VERIFY:
            g_dss_formatter_opts.debug = false;
            break;
        case DSS_FORMATTER_OPTION_DEV_NAME:
            g_dss_formatter_opts.dev_name = strdup(optarg);
            break;
        default:
            usage();
            return 1;
    }
    return 0;
}

static void
dss_format_run(void *arg /*Not used*/)
{

    // Init and run formatter application
    formatter_run_cmdline(&g_dss_formatter_opts);

    SPDK_NOTICELOG("dss-format completed\n");
    return;
}

char rpc_sock_fname[256];

int
main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 1;

    char *replace_ptr;

    spdk_app_opts_init(&opts);
    opts.name = "dss_format";
    opts.reactor_mask = "0x1";
    opts.config_file = "./spdk_conf";
    opts.shutdown_cb = NULL;

    dss_formatter_set_default_opts(&g_dss_formatter_opts);

	rc = spdk_app_parse_args(argc, argv, &opts, "", g_cmdline_opts, dss_formatter_parse_args, usage);
	if (rc == SPDK_APP_PARSE_ARGS_FAIL) {
		SPDK_ERRLOG("Invalid arguments\n");
		return 4;
	} else if (rc == SPDK_APP_PARSE_ARGS_HELP) {
		return 0;
	}

    if( false == dss_formatter_validate_config(&g_dss_formatter_opts)) {
        usage();
        SPDK_ERRLOG("dss-format failed due to invalid config\n");
        return 5;
    }

    printf("-------------------------------------------------------------\n");
    printf("Formatting with options:\n");
    printf("-------------------------------------------------------------\n");
    printf("\tDevice             : %s\n", g_dss_formatter_opts.dev_name);
    printf("\tBlock size         : %d\n", g_dss_formatter_opts.blk_size);
    printf("\tTotal block states : %d\n", g_dss_formatter_opts.nblk_states);
    printf("\tAllocator type     : %s\n", g_dss_formatter_opts.ba_type);
    printf("==============================================================\n");

    snprintf(rpc_sock_fname, 255, "/var/tmp/dss_formatter.%s.sock", g_dss_formatter_opts.dev_name);
    replace_ptr = rpc_sock_fname;
    while(replace_ptr) {//Replace all space with _
        replace_ptr = strstr(replace_ptr, " ");
        if(replace_ptr) {
            replace_ptr[0] = '_';
        }
    }
    opts.rpc_addr = rpc_sock_fname;

    rc = spdk_app_start(&opts, dss_format_run, NULL);
    if (rc) {
        SPDK_ERRLOG("Error occured while running '%s' app\n", argv[0]);
        return rc;
    }

    SPDK_NOTICELOG("Closing application dss-formatter\n");
    
    spdk_app_fini();
    return rc;
}
