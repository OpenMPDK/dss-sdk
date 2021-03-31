#!/usr/bin/python3
#
#
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

import argparse
import ast
import zmq
import sys
import os
import signal
import platform
import time
import json
import logging
import socket
from netifaces import interfaces, ifaddresses, AF_INET


PORT_FRONTEND   = "6000"
PORT_BACKEND    = "6001"
PORT_VALIDATION = "6002"
DELAY           = 1
RESEND_MAX      = 2
POLLIN_MAX      = 2
POOLIN_INTERVAL = 500
PLATFORM        = platform.node().split('.', 1)[0]


logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(name)s:%(message)s')

file_handler = None


is_main_running = True
def signal_handler(sig, frame):
    global is_main_running
    is_main_running = False


def get_ip_of_nic():
    '''
     Return the first valid nic ip
     else return localhost ip (127.0.0.1)
    '''
    localhost=None
    for ifaceName in interfaces():
        addresses = [i['addr'] for i in ifaddresses(ifaceName).setdefault(AF_INET, [{'addr':'No IP addr'}] )]
        if ifaceName == 'lo':
            localhost=addresses[0]
        else:
            # print('{}: {}'.format(ifaceName, ', '.join(addresses)) )
            return addresses[0]

    return localhost


def str2bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true'):
        return True
    elif v.lower() in ('no', 'false'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


def broker_main():
    global logger
    global file_handler

    if os.access('/var/log/ufm_msg_broker', os.W_OK):
        file_handler = logging.FileHandler('/var/log/ufm_msg_broker/ufm_msg_broker.log')
    else:
        file_handler = logging.FileHandler('ufm_msg_broker.log')

    file_handler.setFormatter(formatter)

    logger.addHandler(file_handler)

    parser = argparse.ArgumentParser(description='UFM broker')
    parser.add_argument("--listen_on_any", help="Listen on any IP address", type=str2bool, nargs='?', const=True, default=False)

    args = parser.parse_args()

    proxy_ip = None
    if args.listen_on_any:
        # Listens to any IP address
        proxy_ip = "tcp://" + "0.0.0.0"
    else:
        proxy_ip = "tcp://" + get_ip_of_nic()

    logger.info("Using " + proxy_ip)

    context = zmq.Context()

    # Frontend receive events from Cluster monitor
    frontend  = context.socket(zmq.REP)
    frontend.bind("{}:{}".format(proxy_ip, PORT_FRONTEND))

    # Backend send the events to the subscribers.
    backend  = context.socket(zmq.PUB)
    backend.bind("{}:{}".format(proxy_ip, PORT_BACKEND))
    backend.getsockopt(zmq.SNDHWM)
    backend.getsockopt(zmq.RCVHWM)
    backend.HEARTBEAT_IVL = 5000
    backend.HEARTBEAT_TIMEOUT = 50000

    logger.info("ZeroMQ Broker started at {}: VIP-{}:{}".format(PLATFORM, proxy_ip, PORT_FRONTEND))

    global is_main_running
    while is_main_running:
        try:
            # Receive message
            msg = frontend.recv()
            (channel, event) = ast.literal_eval(msg.decode("utf-8"))
            logger.debug("Received channel={} event={}".format(channel, event))
            time.sleep(DELAY)

            # Publish events to the subscriber
            backend.send_string("{}={}".format(channel, json.dumps(event)))

            # Send acknowledgement to the monitor( event_notification(
            frontend.send_string("RECEIVED", zmq.NOBLOCK)
        except Exception as ex:
            logger.exception("ZMQ broker died: {}".format(str(ex)))
            break

    # Close context
    frontend.close()
    backend.close()
    context.term()
    logger.info("ZMQ broker stopped")


if __name__ == "__main__":
    broker_main()

