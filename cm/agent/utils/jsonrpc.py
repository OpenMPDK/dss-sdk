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
