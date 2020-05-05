#!/usr/bin/python

import ast
import zmq
import sys,os
import platform
import time
import json
from logger import cm_logger

logger = cm_logger.CMLogger("event_notification").create_logger()

PROXY_IP        = "tcp://*"
PORT_FRONTEND   = "6000"
PORT_BACKEND    = "6001"
PORT_VALIDATION = "6002"
DELAY           = 1
RESEND_MAX      = 2
POLLIN_MAX      = 2
POOLIN_INTERVAL = 500
PLATFORM        = platform.node().split('.', 1)[0]

def proxy():
    context = zmq.Context()
    logger.info("ZeroMQ Broker started at {}: VIP-{}:{}".format(PLATFORM, PROXY_IP, PORT_FRONTEND))

    # Frontend receive events from Cluster monitor
    frontend  = context.socket(zmq.REP)
    frontend.bind("{}:{}".format(PROXY_IP, PORT_FRONTEND))

    # Backend send the events to the subscribers.
    backend  = context.socket(zmq.PUB)
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
            print(channel,event)
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

