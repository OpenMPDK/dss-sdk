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


#
# Description:
#   This program is a test utility that listen for events
#   that ufm_broker receives and validates the events.
#
#   The utility intended to be use in regression test and
#   as well in development e.g. in a development case the
#   flag forever can be used to see all the events that
#   the ufm_broker is getting
#
import platform
import socket
import time
import sys
import argparse

import signal
import threading

import time
from datetime import datetime
from datetime import timedelta

import json
import zmq


g_event = threading.Event()

def signal_handler(sig, frame):
    global g_event
    g_event.set()


def is_event_name_known(event_name):

    known_event_names=["360", "CM_UP", "CM_DOWN", "SUBSYSTEM_UP",
        "SUBSYSTEM_DOWN", "AGENT_UP", "AGENT_DOWN", 'CM_NODE_UP',
        'CM_NODE_DOWN', 'CM_NODE_ADDED', 'CM_NODE_REMOVED',
        'CM_NODE_REST_UP', 'CM_NODE_REST_DOWN', 'CM_NODE_MONITOR_UP',
        'CM_NODE_MONITOR_DOWN', 'AGENT_STOPPED', 'AGENT_RESTARTED',
        'TARGET_APPLICATION_UP', 'TARGET_APPLICATION_DOWN',
        'TARGET_APPLICATION_STOPPED', 'TARGET_DISK', 'TARGET_NIC',
        'TARGET_NODE_ACCESSIBLE', 'TARGET_NODE_UNREACHABLE',
        'TARGET_NODE_INTIALIZED', 'TARGET_NODE_REMOVED', 'SUBSYSTEM_CREATED',
        'SUBSYSTEM_DELETED', 'NETWORK_UP', 'NETWORK_DOWN', 'DB_INSTANCE_DOWN',
        'DB_INSTANCE_UP', 'DB_CLUSTER_HEALTHY', 'DB_CLUSTER_DEGRADED',
        'DB_CLUSTER_FAILED', 'STATS_DB_DOWN', 'STATS_DB_UP',
        'GRAPHITE_DB_DOWN', 'GRAPHITE_DB_UP', 'SOFTWARE_UPGRADE_SUCCESS',
        'SOFTWARE_UPGRADE_FAILED', 'SOFTWARE_DOWNGRADE_SUCCESS',
        'SOFTWARE_DOWNGRADE_FAILED']

    return event_name in known_event_names


def subscribe(event, ip, port, listen_forever=False, timeout=0, expected_event_category=None, expected_event_name=None, expected_count=1):
    event.clear()

    connect_str = "tcp://" + ip + ":" + port
    fqdn = socket.getfqdn(ip)

    context = zmq.Context()
    so = context.socket(zmq.SUB)
    so.setsockopt(zmq.IPV6, 1)

    so.setsockopt_string(zmq.SUBSCRIBE,'')
    so.connect(connect_str)

    start_time = datetime.now()

    print("="*70)
    rc = 1
    found_event = 0
    while not event.is_set():
        message=""
        test_pass=False
        fail_message=""

        while not event.is_set():
            if timeout != 0:
                if datetime.now() > start_time + timedelta(seconds = timeout):
                    fail_message += "Test timedout "
                    print(" {}   FAIL".format(fail_message))
                    break

            try:
                message = so.recv_string(flags=zmq.NOBLOCK)
            except zmq.ZMQError as e:
                if e.errno == zmq.EAGAIN:
                    continue

            if (len(message) == 0):
                continue

            # The messages from the broker are expected to
            # have this format <category>=<jsonblob>
            (category, json_event) = message.split('=')

            d=json.loads(json_event)

            if expected_event_category != "":
                if expected_event_category.upper() != category.upper():
                    fail_message += "category didn't match  "

            if 'name' in json_event:
                event_name=d['name']

                if not is_event_name_known(event_name):
                    fail_message += "Unknown event name  "
                else:
                    print("{} = {} ".format(category, event_name), end="" )
                    if event_name == expected_event_name:
                        found_event += 1
                        print("   PASS Found {}".format(str(found_event)))
                        if found_event >= int(expected_count):
                            test_pass=True
                            rc=0
                            break
                    else:
                        print("")
            else:
                # dump the whole json msg, if the 'name' field doesn't exist
                print("{} = {} ".format(category, json.dumps(d, sort_keys=True, indent=2)), end="")
                fail_message += "Missing event name  "

        print("="*70)

        if not test_pass:
            return rc

        if not listen_forever:
            return rc

    return rc


def str2bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true'):
        return True
    elif v.lower() in ('no', 'false'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


def main():
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGQUIT, signal_handler)

    parser = argparse.ArgumentParser(description='Validate events that the broker receives')

    parser.add_argument("--host", help="localhost or nic IP", dest="host", default="localhost", required=False)
    parser.add_argument("--port", help="Port number", dest="port", default="6001")
    parser.add_argument("--forever", help="Listen for events forever (for debugging)", type=str2bool, nargs='?', const=True, default=False)
    parser.add_argument("--expected_event_name", help="Name of the expected event", default="", dest="expected_event_name")
    parser.add_argument("--expected_event_category", help="Name of the expected event", default="", dest="expected_event_category")
    parser.add_argument("--expected_count", help="Number of expected event", dest="expected_count", default="1")
    parser.add_argument("--timeout", help="Listen for max <timeout> seconds", dest="timeout", default="0")

    args = parser.parse_args()

    hostname = socket.gethostbyname(args.host)

    return subscribe(
            event=g_event,
            ip=hostname,
            port=args.port,
            listen_forever=args.forever,
            timeout=int(args.timeout),
            expected_event_category=args.expected_event_category,
            expected_event_name=args.expected_event_name,
            expected_count=args.expected_count
    )


if __name__ == "__main__":
    sys.exit(main())


