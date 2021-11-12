SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../oss/spdk_tcp)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

APP = test_rdd

C_SRCS = $(APP:%=%.c)

SPDK_LIB_LIST += thread util log trace conf event rpc jsonrpc json net sock
SPDK_LIB_LIST += app_rpc log_rpc

COMMON_CFLAGS += -I$(SPDK_ROOT_DIR)/../../include
LIBS += -L$(SPDK_ROOT_DIR)/../../ -loss

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
