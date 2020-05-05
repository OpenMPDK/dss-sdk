import socket

# CM Verson
CM_VERSION=0.3

# CM state types
CM_UP = 0
CM_DOWN = 1
CM_BOOTING = 2

# CM mode types
CM_ACTIVE = 0
CM_PASSIVE = 1

# Subsystem types
SUBSYSTEM_OK = 0
SUBSYSTEM_DOWN = 1

# Health types
HEALTH_OK = 0
HEALTH_DEGRADED = 1
HEALTH_RECOVERING = 2
HEALTH_REBALANCING = 3
HEALTH_WARNING = 4
HEALTH_CRITICAL = 5
HEALTH_FAILED = 6

# Subsystem types
SUBSYSTEM_TYPE_TCP = 0
SUBSYSTEM_TYPE_RDMA = 1

# Subsystem Address family enum values
SUBSYSTEM_ADDR_FAMILY_IPV4 = socket.AF_INET
SUBSYSTEM_ADDR_FAMILY_IPV6 = socket.AF_INET6

# Subsystem interface status
SUBSYSTEM_IFACE_STATUS_DOWN = 0
SUBSYSTEM_IFACE_STATUS_UP = 1

# Network interface speed
# Speed to enumerated value
NET_SPEED_DICT = {1: 0, 10:1, 50:2, 100:3}
'''
LS_1GBIT = 0
LS_10GBIT = 1
LS_50GBIT = 2
LS_100GBIT = 3
'''

# Maximum time lapse across heartbeat updates
MAX_TIME_LAPSE = 90

# Event type definitions
CLUSTER_CONFIG_CHANGE = 0
CLUSTER_STATE_CHANGE = 1
CLUSTER_CLIENT_CHANGE = 2

# Event Severity definitions
INFO = 0
WARNING = 1
ERROR = 2
CRITICAL = 3
