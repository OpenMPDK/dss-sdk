#####################################################################################
#   BSD LICENSE
#
#   Copyright (c) 2021 Samsung Electronics Co., Ltd.
#   All rights reserved.#
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
#       its contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#####################################################################################

"""
Usage:
kv-cli.py log list [options]
kv-cli.py log tgt get_trace_flags
kv-cli.py log tgt clear_trace_flags [--flag=<flag>]
kv-cli.py log tgt set_trace_flags [--flag=<flag>]
kv-cli.py log tgt get_log_level
kv-cli.py log tgt set_log_level --level=<level>
kv-cli.py log tgt get_log_print_level
kv-cli.py log tgt set_log_print_level --level=<level>

Options:
  -h --help          Show this screen.
  --flag=<flag>      Flag to enable or clear trace [default: all]
                     Flags supported ['vbdev_lvol', 'virtio_pci', 'rpc', 'lvol', 'nbd', 'log', 'gpt_parse',
                     'ioat', 'nvmf', 'nvme', 'lvolrpc', 'aio', 'blob', 'virtio_dev', 'bdev_nvme', 'reactor',
                     'virtio_user', 'virtio', 'bdev_null', 'vbdev_gpt', 'bdev_malloc', 'vbdev_split', 'rdma',
                     'bdev', 'blob_rw']
  --level=<level>    Log level to enable [ default: 'NOTICE']
                     Log levels supported ['DEBUG', 'INFO', 'NOTICE', 'WARNING', 'ERROR']
"""
import json
import sys

import utils.daemon_config as agent_conf
from utils.jsonrpc import SPDKJSONRPC as SpdkJsonRPC


def display_log_settings():
    '''
    Displays list of NVMe devices on the system it's executed on.
    :return:
    '''
    agent_conf.create_config(agent_conf.CONFIG_DIR + agent_conf.AGENT_CONF_NAME)
    cfg_settings = agent_conf.load_config(agent_conf.CONFIG_DIR + agent_conf.AGENT_CONF_NAME)
    if 'logging' in cfg_settings:
        logger_setttings = cfg_settings['logging']
        print(json.dumps(logger_setttings, indent=2))
        for setting in logger_setttings:
            print('%-20s %s' % (setting + ':', logger_setttings[setting]))


def get_rpc_method(spdk_sock, buf_size, method):
    try:
        rpc_req = SpdkJsonRPC.build_payload(method)
        results = SpdkJsonRPC.call(rpc_req, buf_size, spdk_sock)
        if 'error' in results:
            print('Method <%s> failed with error <%s>' % (method, results['error']['message']))
        else:
            print(results['result'])
    except Exception as e:
        print('Received an exception ', e.message)


def set_rpc_method(spdk_sock, buf_size, method, params):
    try:
        rpc_req = SpdkJsonRPC.build_payload(method, params)
        results = SpdkJsonRPC.call(rpc_req, buf_size, spdk_sock)
        if 'error' in results:
            print('Method <%s> failed with error <%s>' % (method, results['error']['message']))
        else:
            print(results['result'])
    except Exception as e:
        print('Received an exception ', e.message)


def main(args):
    if args['log']:
        if args['list']:
            display_log_settings()
        elif args['tgt']:
            spdk_sock = '/var/run/spdk.sock'
            buf_size = 4096
            params = None
            if args['get_trace_flags']:
                fn = get_rpc_method
                method = 'get_trace_flags'
            elif args['set_trace_flags']:
                fn = set_rpc_method
                method = 'set_trace_flag'
                params = {'flag': args['--flag']}
            elif args['clear_trace_flags']:
                fn = set_rpc_method
                method = 'clear_trace_flag'
                params = {'flag': args['--flag']}
            elif args['get_log_level']:
                fn = get_rpc_method
                method = 'get_log_level'
            elif args['set_log_level']:
                fn = set_rpc_method
                method = 'set_log_level'
                params = {'level': args['--level']}
            elif args['get_log_print_level']:
                fn = get_rpc_method
                method = 'get_log_print_level'
            elif args['set_log_print_level']:
                fn = set_rpc_method
                method = 'set_log_print_level'
                params = {'level': args['--level']}
            else:
                print('Invalid option provided')
                sys.exit(-1)

            if params:
                fn(spdk_sock, buf_size, method, params)
            else:
                fn(spdk_sock, buf_size, method)
