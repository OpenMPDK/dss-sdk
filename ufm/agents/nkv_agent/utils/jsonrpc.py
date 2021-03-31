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

import json
import socket

from log_setup import agent_logger


class JSONRPCException(Exception):
    pass


class SPDKJSONRPC:
    def __init__(self):
        pass

    @staticmethod
    def build_payload(method, params=None):
        """
        Create JSON-RPC request dictionary object.
        :param method: SPDK command.
        :param params: Required parameters for the provided command.
        :return: JSON-RPC request as dictionary object.
        """
        req = {'jsonrpc': '2.0',
               'method': method,
               'id': 1, }
        if params is not None:
            req["params"] = params
        return req

    @staticmethod
    def call(payload, recv_sz, socket_path):
        """
        Send JSON-RPC request to local SPDK socket.
        :param payload: JSON-RPC request as dictionary object.
        :param recv_sz: Size of each packet to receive.
        :param socket_path: Path to local UNIX socket.
        :return: JSON-RPC response.
        """
        if payload['method'] not in ['dfly_oss_version_info']:
            raise JSONRPCException('TEMP - No response from JSON RPC call for %s',
                                   json.dumps(payload))

        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(socket_path)
        s.sendall(json.dumps(payload))

        response = None
        data = b''
        while 1:
            packet = s.recv(recv_sz)
            if not packet:
                break
            data += packet
            try:
                response = json.loads(data)
            except ValueError:
                continue
            break
        s.close()

        if not response:
            if agent_logger:
                agent_logger.exception("Connection closed with partial  "
                                       "response: req %s, res %s",
                                       json.dumps(payload), data)
            else:
                print("Connection closed with partial response: " + data)
            raise JSONRPCException('No response from JSON RPC call for %s',
                                   json.dumps(payload))
        elif 'error' in response:
            if agent_logger:
                agent_logger.error('Received error %s for the call %s',
                                   str(response), str(payload))
            else:
                print('Received error ' + str(response) + ' for the call' +
                      str(payload))

        return response
