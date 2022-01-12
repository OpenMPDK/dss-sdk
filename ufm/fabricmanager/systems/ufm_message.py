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


import time
import threading
import zmq
import json


class Subscriber(threading.Thread):
    def __init__(self, event=None, ports=None, topics=None, process=None):
        self.event = event
        self.ports = ports
        self.topicfilter = topics
        if not process:
            self.process = self.debug
        else:
            self.process = process
        super(Subscriber, self).__init__()
        self.context = zmq.Context()

    def __del__(self):
        self.event.set()
        self.context.destroy()

    def run(self):
        socket = self.context.socket(zmq.SUB)
        socket.setsockopt(zmq.IPV6, 1)

        for port in self.ports:
            socket.connect("tcp://localhost:{}".format(port))

        for topic in self.topicfilter:
            socket.setsockopt_string(zmq.SUBSCRIBE, topic)

        # self.event.clear()
        while not self.event.is_set():
            try:
                string = socket.recv_string(flags=zmq.NOBLOCK)
            except KeyboardInterrupt:
                break
            except Exception:
                time.sleep(0.5)
                continue

            topic, message = string.split('=')

            self.process(topic, message)

        socket.close()

    def stop(self):
        self.event.set()

    def debug(self, topic, message):
        print("{} {}".format(topic, message))


class Publisher():
    def __init__(self, port=None):
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        self.socket.setsockopt(zmq.IPV6, 1)
        self.socket.bind("tcp://*:{}".format(self.port))

    def __del__(self):
        if self.socket:
            self.socket.close()

        self.context.destroy()

    def send(self, topic=None, jsonMessage=None):
        if not topic:
            return

        if not jsonMessage:
            return

        if type(jsonMessage) is dict:
            tmpString = json.dumps(jsonMessage)
            self.socket.send_string("{}={}".format(topic, tmpString))
