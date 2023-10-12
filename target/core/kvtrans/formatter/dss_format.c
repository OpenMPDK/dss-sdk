#include "spdk/stdinc.h"

#include "spdk/bdev.h"
#include "spdk/event.h"
//#include "formatter.h"
#include "formatter_interface.h"

static void usage(void) {
    printf("[--------- Usage ---------]\n");
    printf("./dss_formatter <conf-file> <parse-mode>\n");
    printf("<parse-mode>: TEXT | <other modes>\n");
    printf("[------------------------]\n");

    return;
}

static void init(formatter_conf_t *conf) {

    // Init and run formatter application
    formatter_init(conf);

    return;
}

static void
dss_format(void *arg)
{
    formatter_conf_t *conf = (formatter_conf_t *)arg;
    usage();
    init(conf);

    SPDK_NOTICELOG("dss-format completed\n");
    return;
}

int
main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc = 1;
    spdk_app_opts_init(&opts);
    opts.name = "dss_format";
    opts.reactor_mask = "0x1";
    opts.config_file = "./spdk_conf";
    opts.shutdown_cb = NULL;

    formatter_conf_t *conf =
        (formatter_conf_t *)malloc(sizeof(formatter_conf_t));
    if (conf == NULL) {
        SPDK_ERRLOG("Error occured while running sample app\n");
    }
    memset(conf, 0, sizeof(formatter_conf_t));
    conf->argc = argc;
    conf->argv = argv;

    rc = spdk_app_start(&opts, dss_format, conf);
    if (rc) {
        SPDK_ERRLOG("Error occured while running sample app\n");
        return rc;
    }

    SPDK_NOTICELOG("Closing application dss-formatter\n");
    
    spdk_app_fini();
    return rc;
}
