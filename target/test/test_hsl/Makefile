SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../oss/spdk_tcp)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

APP = test_hsl

C_SRCS = $(APP:%=%.c)

SPDK_LIB_LIST += thread util log

COMMON_CFLAGS += -I$(SPDK_ROOT_DIR)/../../include
LIBS += -L$(SPDK_ROOT_DIR)/../../ -ldss_hsl -ljudyL

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
