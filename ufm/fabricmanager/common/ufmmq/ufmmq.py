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

import importlib

from common.ufmlog import ufmlog
from functools import wraps

#MessageQueue module to use
mq = None

class Ufmmq(object):
    def __new__(cls, **kwargs):
        """
        Get the message queue type, load the required module and
        validate the inputs to the message queue connection.
        """
        #Change the message queue to use based on the inputs
        global mq
        mq_type = kwargs.pop('mq_type', 'zmq')
        if mq_type == 'zmq':
            mq = importlib.import_module('common.ufmmq.ufmmq_zmq.ufmmq_zmq_client')
        else:
            raise Exception('Invalid message queue type provided, {} is not valid'.format(mq_type))

        # Create an instance of the class only if the input options are valid
        if mq.is_valid_init_args(**kwargs):
            return object.__new__(cls)
        else:
            raise Exception('Invalid inputs provided')

    def __init__(self, **kwargs):
        """
        Create a context for the message queue.

        :param kwargs: additional options dictionary
        :param mq_type: message queue type to use.
            :type mq_type: str, Default: 'zmq'
        """
        self.log = ufmlog.log(module="MQ", mask=ufmlog.UFM_MQ)
        self.log.log_detail_off()

        self.client = mq.client(**kwargs)

    def log_error(func):
        def func_wrapper(*args, **kwargs):
            self = args[0]
            try:
                return func(*args, **kwargs)
            except Exception as e:
                self.log.exception(e)
                return None
        return func_wrapper

    def term(self):
        """Terminate context."""
        if (self.client != None):
            self.client.term()

    @log_error
    def create_socket(self, socket_type):
        """
        Create a socket associated with this context.
        Can create multiple sockets within one context.
        """
        self.log.detail("MQ CREATE_SOCKET: socket_type=" + str(socket_type))
        return self.client.create_socket(socket_type)

    def close_socket(self, socketObj):
        """Close the socket."""
        self.log.detail("MQ CLOSE_SOCKET:")
        self.client.close_socket(socketObj)

    def getsockopt(self, socketObj, option):
        """Get the value of a socket option."""
        self.log.detail("MQ GET_SOCKET_OPTION: option=" + str(option))
        return self.client.getsockopt(socketObj, option)

    def setsockopt(self, socketObj, option, optVal):
        """Set the value of a socket option."""
        self.log.detail("MQ SET_SOCKET_OPTION: option=%s optVal=%s" % (str(option), optVal))
        return self.client.setsockopt(socketObj, option, optVal)

    def bind_socket(self, socketObj, addr, port):
        """Bind the socket to an address."""
        self.log.detail("MQ BIND_SOCKET: addr=%s port=%s" % (addr, port))
        self.client.bind_socket(socketObj, addr, port)

    def connect_socket(self, socketObj, addr, port):
        """Connect to a remote socket."""
        self.log.detail("MQ CONNECT_SOCKET: addr=%s port=%s" % (addr, port))
        self.client.connect_socket(socketObj, addr, port)

    def send(self, socketObj, data):
        """Send a single message on this socket."""
        self.log.detail("MQ SEND: data=%s", data)
        return self.client.send(socketObj, data)

    def receive(self, socketObj):
        """Receive a message on this socket."""
        self.log.detail("MQ RECEIVE")
        msg = self.client.receive(socketObj)
        self.log.detail("MQ RECEIVED: %s", msg)
        return msg

    def subscribe_socket(self, socketObj, topic):
        """Subscribe a socket to a topic."""
        self.log.detail("MQ SUBSCRIBE: topic=%s", str(topic))
        self.client.subscribe_socket(socketObj, topic)

    def create_poller(self, timeout=None):
        """Poll the registered zmq for I/O."""
        self.log.detail("MQ CREATE_POLLER")
        return self.client.create_poller(timeout)

    def register_poller(self, pollerObj, flags):
        """Register a poller for event monitoring."""
        self.log.detail("MQ REGISTER_POLLER")
        self.client.register_poller(pollerObj, flags)

def client(**kwargs):
    """Return an instance of Ufmmq_Client."""
    return Ufmmq(**kwargs)




