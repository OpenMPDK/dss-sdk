"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

# The event ID string represents the following format
# <Component ID><Severity><Event number within the component>
# For example, event ID with 01001000 can represent the component with
# component ID "01", severity "01" and the event index "1000"

# Component IDs
#   -------------------------------------
#   |   Name                |           |
#   -------------------------------------
#   | CM Nodes              |   01      |
#   | Agent                 |   02      |
#   | Target                |   03      |
#   | Subsystem             |   04      |
#   | Datapath              |   05      |
#   | Network               |   06      |
#   | Disk                  |   07      |
#   | ETCD DB               |   08      |
#   | Services              |   09      |
#   | Software Upgrade      |   10      |
#   -------------------------------------

# Severity
#   -------------------------------------
#   | INFO                  |   01      |
#   | WARNING               |   02      |
#   | ERRO                  |   03      |
#   | CRITICAL              |   04      |
#   -------------------------------------

# Event Type
#   -------------------------------------
#   | CLUSTER_CLUSTER_CHANGE  |   00     |
#   | CLUSTER_STATE_CHANGE    |   01     |
#   | CLUSTER_CLIENT_CHANGE   |   02     |
#   -------------------------------------

components = ['NONE', 'CM_NODE', 'AGENT', 'TARGET', 'SUBSYSTEM',
              'DATAPATH', 'NETWORK', 'DISK', 'ETCDDB', 'SERVICE']

severity = ['NONE', 'INFO', 'WARN', 'CRIT']
event_type = {'config': 0, 'state': 1, 'client': 2}
# All the function hooks in global_event_handlers will be run
global_event_handlers = None

events = {
    'CM_NODE_UP': {
        'id': '01011000',
        'msg': 'CM node {node} in cluster {cluster} is up',
        'cluster_change': 'config',
        'handler': None,
        'category': 'CM_NODE'
    },
    'CM_NODE_DOWN': {
        'id': '01021001',
        'msg': 'CM node {node} in cluster {cluster} is down',
        'cluster_change': 'config',
        'handler': None,
        'category': 'CM_NODE'
    },
    'CM_NODE_ADDED': {
        'id': '01011002',
        'msg': 'CM node {node} is added to cluster {cluster}',
        'cluster_change': 'config',
        'handler': None,
        'category': 'CM_NODE'
    },
    'CM_NODE_REMOVED': {
        'id': '01011003',
        'msg': 'CM node {node} is removed from cluster {cluster}',
        'cluster_change': 'config',
        'handler': None,
        'category': 'CM_NODE'
    },
    'CM_NODE_REST_UP': {
        'id': '01011004',
        'msg': 'REST API Server on CM node {node} in cluster {cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'CM_NODE_REST'
    },
    'CM_NODE_REST_DOWN': {
        'id': '01021005',
        'msg': 'REST API Server on CM node {node} in cluster {cluster} is '
               'down',
        'cluster_change': 'state',
        'handler': None,
        'category': 'CM_NODE_REST'
    },
    'CM_NODE_MONITOR_UP': {
        'id': '01011006',
        'msg': 'Monitor on CM node {node} in cluster {cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'CM_NODE_MONITOR'
    },
    'CM_NODE_MONITOR_DOWN': {
        'id': '01021007',
        'msg': 'Monitor on CM node {node} in cluster {cluster} is down',
        'cluster_change': 'state',
        'handler': None,
        'category': 'CM_NODE_MONITOR'
    },
    'AGENT_UP': {
        'id': '02011000',
        'msg': 'Agent on node {node} in cluster {cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'AGENT'
    },
    'AGENT_DOWN': {
        'id': '02011001',
        'msg': 'Agent on node {node} in cluster {cluster} is down',
        'cluster_change': 'state',
        'handler': None,
        'category': 'AGENT'
    },
    'AGENT_STOPPED': {
        'id': '01021002',
        'msg': 'Agent on node {node} in cluster {cluster} stopped gracefully',
        'cluster_change': 'state',
        'handler': None,
        'category': 'AGENT'
    },
    'AGENT_RESTARTED': {
        'id': '02021002',
        'msg': 'Agent on node {node} in cluster {cluster} stopped '
               'unexpectedly and restarted',
        'cluster_change': 'state',
        'handler': None,
        'category': 'AGENT'
    },
    'TARGET_APPLICATION_UP': {
        'id': '03011000',
        'msg': 'Target application on node {node} in cluster {cluster} is up',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET',
    },
    'TARGET_APPLICATION_DOWN': {
        'id': '03011001',
        'msg': 'Target application on node {node} in cluster {cluster} is '
               'down',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET'
    },
    'TARGET_APPLICATION_STOPPED': {
        'id': '03011002',
        'msg': 'Target application on node {node} in cluster {cluster} is '
               'stopped gracefully',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET'
    },
    'TARGET_DISK': {
        'id': '03011002',
        'msg': 'Target application on node {node} in cluster {cluster} is '
               'stopped gracefully',
        'cluster_change': 'config',
        'handler': None,
        'category': 'DISK'
    },
    'TARGET_NIC': {
        'id': '03011002',
        'msg': 'Target application on node {node} in cluster {cluster} is '
               'stopped gracefully',
        'cluster_change': 'config',
        'handler': None,
        'category': 'NIC'
    },
    'TARGET_NODE_ACCESSIBLE': {
        'id': '03021003',
        'msg': 'Target node {node} in cluster {cluster} is accessible',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET_NODE'
    },
    'TARGET_NODE_UNREACHABLE': {
        'id': '03021004',
        'msg': 'Target node {node} in cluster {cluster} is unreachable',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET_NODE'
    },
    'TARGET_NODE_INTIALIZED': {
        # Target node setup with agent and target as part of deployment
        'id': '03011005',
        'msg': 'Target node {node} in cluster {cluster} is initialized',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET_NODE'
    },
    'TARGET_NODE_REMOVED': {
        'id': '03011006',
        'msg': 'Target node {node} in cluster {cluster} is removed from '
               'cluster',
        'cluster_change': 'config',
        'handler': None,
        'category': 'TARGET_NODE'
    },
    'SUBSYSTEM_UP': {
        'id': '04011000',
        'msg': 'Subsystem for target {nqn} on node {node} in cluster {'
               'cluster} is up',
        'cluster_change': 'config',
        'handler': None,
        'category': 'SUBSYSTEM'
    },
    'SUBSYSTEM_DOWN': {
        'id': '04011001',
        'msg': 'Subsystem for target {nqn} on node {node} in cluster {'
               'cluster}  is down',
        'cluster_change': 'config',
        'handler': None,
        'category': 'SUBSYSTEM'
    },
    'SUBSYSTEM_CREATED': {
        'id': '04011002',
        'msg': 'Subsystem for target {nqn} on node {node} in cluster {'
               'cluster} is created',
        'cluster_change': 'config',
        'handler': None,
        'category': 'SUBSYSTEM'
    },
    'SUBSYSTEM_DELETED': {
        'id': '04011003',
        'msg': 'Subsystem for target {nqn} on node {node} in cluster {'
               'cluster} is deleted',
        'cluster_change': 'config',
        'handler': None,
        'category': 'SUBSYSTEM'
    },
    'NETWORK_UP': {
        'id': '06011000',
        'msg': 'Network interface {net_interface} on node {node} in cluster {'
               'cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'NETWORK'
    },
    'NETWORK_DOWN': {
        'id': '06021001',
        'msg': 'Network interface {net_interface} on node {node} in cluster {'
               'cluster} is down',
        'cluster_change': 'state',
        'handler': None,
        'category': 'NETWORK'
    },
    'DB_INSTANCE_DOWN': {
        'id': '08021000',
        'msg': 'ETCD DB instance on node {node} in cluster {cluster} is '
               'inaccessible',
        'cluster_change': 'state',
        'handler': None,
        'category': 'ETCD'
    },
    'DB_INSTANCE_UP': {
        'id': '08011001',
        'msg': 'ETCD DB instance on node {node} in cluster {cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'ETCD'
    },
    'DB_CLUSTER_HEALTHY': {
        'id': '08031002',
        'msg': 'ETCD DB cluster {cluster} is healthy',
        'cluster_change': 'state',
        'handler': None,
        'category': 'ETCD'
    },
    'DB_CLUSTER_DEGRADED': {
        'id': '08031003',
        'msg': 'ETCD DB cluster {cluster} is degraded',
        'cluster_change': 'state',
        'handler': None,
        'category': 'ETCD'
    },
    'DB_CLUSTER_FAILED': {
        'id': '08031004',
        'msg': 'ETCD DB cluster {cluster} failed',
        'cluster_change': 'state',
        'handler': None,
        'category': 'ETCD'
    },
    'STATS_DB_DOWN': {
        'id': '08031005',
        'msg': 'Stats DB for cluster {cluster} is inaccessible',
        'cluster_change': 'state',
        'handler': None,
        'category': 'STATS_DB'
    },
    'STATS_DB_UP': {
        'id': '08031006',
        'msg': 'Stats DB for cluster {cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'STATS_DB'
    },
    'GRAPHITE_DB_DOWN': {
        'id': '08031007',
        'msg': 'Graphite DB for cluster {cluster} is inaccessible',
        'cluster_change': 'state',
        'handler': None,
        'category': 'GRAPHITE'
    },
    'GRAPHITE_DB_UP': {
        'id': '08031008',
        'msg': 'Graphite DB for cluster {cluster} is up',
        'cluster_change': 'state',
        'handler': None,
        'category': 'GRAPHITE'
    },
    'SOFTWARE_UPGRADE_SUCCESS': {
        'id': '100110001',
        'msg': 'Software upgrade to image {image} successful',
        'cluster_change':  'state',
        'handler': None,
        'category': 'UPGRADE'
    },
    'SOFTWARE_UPGRADE_FAILED': {
        'id': '100310002',
        'msg': 'Software upgrade to image {image} failed',
        'cluster_change': 'state',
        'handler': None,
        'category': 'UPGRADE'
    },
    'SOFTWARE_DOWNGRADE_SUCCESS': {
        'id': '100110003',
        'msg': 'Software downgrade to image {image} successful',
        'cluster_change': 'state',
        'handler': None,
        'category': 'UPGRADE'
    },
    'SOFTWARE_DOWNGRADE_FAILED': {
        'id': '100310004',
        'msg': 'Software downgrade to image {image} failed',
        'cluster_change': 'state',
        'handler': None,
        'category': 'UPGRADE'
    },
}
