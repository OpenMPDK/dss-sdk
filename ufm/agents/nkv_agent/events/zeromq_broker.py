#!/usr/bin/python

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


import ast
import zmq
import sys
import os
import platform
import time
import json
from logger import cm_logger

logger = cm_logger.CMLogger("event_notification").create_logger()

PROXY_IP = "tcp://*"
PORT_FRONTEND = "6000"
PORT_BACKEND = "6001"
PORT_VALIDATION = "6002"
DELAY = 1
RESEND_MAX = 2
POLLIN_MAX = 2
POOLIN_INTERVAL = 500
PLATFORM = platform.node().split('.', 1)[0]


def proxy():
    context = zmq.Context()
    logger.info("ZeroMQ Broker started at {}: VIP-{}:{}".format(PLATFORM, PROXY_IP, PORT_FRONTEND))

    # Frontend receive events from Cluster monitor
    frontend = context.socket(zmq.REP)
    frontend.bind("{}:{}".format(PROXY_IP, PORT_FRONTEND))

    # Backend send the events to the subscribers.
    backend = context.socket(zmq.PUB)
    backend.bind("{}:{}".format(PROXY_IP, PORT_BACKEND))
    backend.getsockopt(zmq.SNDHWM)
    backend.getsockopt(zmq.RCVHWM)
    backend.HEARTBEAT_IVL = 5000
    backend.HEARTBEAT_TIMEOUT = 50000

    while True:
        try:
            # Receive message
            msg = frontend.recv()
            (channel, event) = ast.literal_eval(msg)
            time.sleep(DELAY)
            print(channel, event)
            # Publish events to the subscriber
            backend.send("{}={}".format(channel, json.dumps(event)))

            # Send acknowledgement to the monitor( event_notification(
            frontend.send("RECEIVED", zmq.NOBLOCK)
        except Exception as e:
            logger.exception(str(e))

    # Close context
    frontend.close()
    backend.close()
    context.term()


if __name__ == "__main__":
    proxy()
