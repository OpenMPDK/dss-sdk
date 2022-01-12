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


import zmq
from common.ufmmq import ufmmq_client

class Ufmmq_zmq_Client(ufmmq_client.UfmmqClient):
    def __init__(self, **kwargs):
        """
        Create a message queue context.

        mq_type is ufmmq specific and should be removed before
        invoking the connection
        """
        kwargs.pop('mq_type','zmq')
        self.client = zmq.Context(io_threads=1, **kwargs)

    def send(self, socketObj, data):
        """
        Send a single message on this socket.

        """
        if (self.client != None):
            if (socketObj != None):
                socketObj.send_string("{}".format(data))

    def receive(self, socketObj):
        """
        Receive a message in bytes from this socket.
        """
        if (self.client != None):
            if (socketObj != None):
                msg = socketObj.recv()
                return msg
        return None

    def term(self):
        """
        Terminate context.

        This can be called to close the context by hand.
        If not called, the context will automatically be closed
        when it is garbage collected.
        """
        if (self.client != None):
            self.client.term()

    def create_socket(self, socket_type):
        """
        Create a Socket associated with this Context.

        Parameters:
            socket_type (int)  The socket type, which can be any of
                the 0MQ socket types: REQ, REP, PUB, SUB, PAIR, DEALER,
                ROUTER, PULL, PUSH, etc.
            return the Socket object.

        Can create multiple sockets associated with one Context.
        """
        if (self.client != None):
            return self.client.socket(socket_type)

    def close_socket(self, socketObj):
        """
        Close the socket.
        """
        if (self.client != None):
            if (socketObj != None):
                socketObj.close()

    def getsockopt(self, socketObj, option):
        """
        Get the value of a socket option.

        Parameters:	option(int). The option to retrieve.
        Returns:	optval. The value of the option as a unicode string.
        Return type:	unicode string (unicode on py2, str on py3)
        """
        if (self.client != None):
            if (socketObj != None):
                return socketObj.getsockopt_string(option)

    def setsockopt(self, socketObj, option, optVal):
        """
        Set socket options with a unicode object.

        Parameters:
            option(int). The name of the option to set. Can be any of: SUBSCRIBE, UNSUBSCRIBE, IDENTITY
            optVal(unicode string (unicode on py2, str on py3)). The value of the option to set.
        """
        if (self.client != None):
            if (socketObj != None):
                socketObj.setsockopt_string(option, optVal)

    def bind_socket(self, socketObj, addr, port):
        """
        Bind the socket to an address.
        This causes the socket to listen on a network port.
        Sockets on the other side of this connection will use Socket.connect(addr)
        to connect to this socket.

        Parameters:     addr (str)  The address string. This has the form protocol://interface:port,
            for example tcp://127.0.0.1:5555. Protocols supported include tcp, udp, pgm, epgm,
            inproc and ipc. If the address is unicode, it is encoded to utf-8 first.
        """
        if (self.client != None):
            if (socketObj != None):
                socketObj.bind("{}:{}".format(addr, port))

    def connect_socket(self, socketObj, addr, port):
        """
        Connect to a remote socket.

        Parameters:     addr (str)  The address string. This has the form protocol://interface:port,
            for example tcp://127.0.0.1:5555. Protocols supported include tcp, udp, pgm, epgm,
            inproc and ipc. If the address is unicode, it is encoded to utf-8 first.
        """
        if (self.client != None):
            if (socketObj != None):
                socketObj.connect("{}:{}".format(addr, port))

    def subscribe_socket(self, socketObj, topic):
        """
        Subscribe to a topic.
        Only for SUB sockets.

        :param topic: interested topic
        :returns: none
        """
        if (self.client != None):
            if (socketObj != None):
                socketObj.subscribe(topic)

    def create_poller(self, timeout=None):
        """
        Poll the registered zmq for I/O.
        Parameters:
            ----timeout (float, int)  The timeout in milliseconds. If None, no timeout (infinite).

        Returns:
            ----events  The list of events that are ready to be processed. This is a list of tuples
            of the form (socket, event), where the 0MQ Socket or integer fd is the first element,
            and the poll event mask (POLLIN, POLLOUT) is the second. It is common to call
            events = dict(poller.poll()), which turns the list of tuples into a mapping of socket : event.

        """
        if (self.client != None):
            return zmq.Poll(timeout)
        else:
            return None

    def register_poller(self, pollerObj, flags=zmq.POLLIN|zmq.POLLOUT):
        """
        Register a socket for I/O monitoring.

        Parameters:
            ----socket (zmq.Socket or native socket)  A zmq.Socket or any Python object having a
            fileno() method that returns a valid file descriptor.
            ----flags (int)  The events to watch for. Can be POLLIN, POLLOUT or POLLIN|POLLOUT. If flags=0, socket will be unregistered.        :param key: key in database to delete
        """
        if (self.client != None):
            if (pollerObj != None):
                return pollerObj.register(self.client, flags)

def is_valid_init_args(**kwargs):
    return True

def client(**kwargs):
    """Return an instance of Ufmmq_zmq_Client."""
    return Ufmmq_zmq_Client(**kwargs)


