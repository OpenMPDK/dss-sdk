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

import zmq
import platform
import socket
import time
import sys
import argparse


def subscribe(ip, port):
    connect_str = "tcp://" + ip + ":" + port
    fqdn = socket.getfqdn(ip)
    print("Subscribing to ZMQ publisher %s from host %s: " % (fqdn, platform.node()))
    ctx = zmq.Context()
    sock = ctx.socket(zmq.SUB)
    sock.setsockopt_string(zmq.SUBSCRIBE,'')
    sock.connect(connect_str)    # FabricManager01

    while(True):
        message = sock.recv_string()
        if (len(message) != 0):
            print(message)
            time.sleep(1)
        else:
            break


def main(argv):
    host_help_msg = "Hostname or IP Address of host running Zero MQ publisher"
    port_help_msg = "Zero MQ publisher port number"
    parser = argparse.ArgumentParser(
                prog='zeromq_subscribe.py', description='Arguments to Zero MQ Subscription', usage='%(prog)s --host <' + host_help_msg + '> [ --port <' + port_help_msg + '> ]')
    parser.add_argument("--host", help = host_help_msg,
                dest="host", default="localhost", required=True)
    parser.add_argument("--port", help = port_help_msg,
                dest="port", default="6001")
    args = parser.parse_args()

    subscribe(socket.gethostbyname(args.host), args.port)


if __name__ == "__main__":
    main(sys.argv[1:])
