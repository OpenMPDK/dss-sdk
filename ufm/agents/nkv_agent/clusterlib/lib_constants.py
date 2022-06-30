# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import socket

# CM Verson
CM_VERSION = 0.3

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
NET_SPEED_DICT = {1: 0, 10: 1, 50: 2, 100: 3}
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
