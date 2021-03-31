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

"""
Template class for message queue clients
"""

class UfmmqClient(object):
    def __init__(self, **kwargs):
        """Create a message queue context."""
        self.client = None
        print("ERROR: UfmmqClient.__init__ must be implemented!")

    def term(self):
        """Terminate context."""
        print("ERROR: UfmmqClient.term must be implemented!")

    def create_socket(self, socket_type):
        """Create a Socket associated with this Context."""
        print("ERROR: UfmmqClient.create_socket must be implemented!")
        return None

    def close_socket(self, socketObj):
        """Close the socket."""
        print("ERROR: UfmmqClient.close_socket must be implemented!")

    def getsockopt(self, socketObj, option):
        """Get the value of a socket option."""
        print("ERROR: UfmmqClient.getsockopt must be implemented!")

    def setsockopt(self, socketObj, option, optVal):
        """Set the value of a socket option."""
        print("ERROR: UfmmqClient.setsockopt must be implemented!")

    def bind_socket(self, socketObj, addr, port):
        """Bind the socket to an address."""
        print("ERROR: UfmmqClient.bind_socket must be implemented!")

    def connect_socket(self, socketObj, addr, port):
        """Connect to a remote socket."""
        print("ERROR: UfmmqClient.connect_socket must be implemented!")

    def send(self, socketObj, data):
        """Send a single message on this socket."""
        print("ERROR: UfmmqClient.send must be implemented!")

    def receive(self, socketObj):
        """Receive a message on this socket."""
        print("ERROR: UfmmqClient.receive must be implemented!")
        return None

    def subscribe_socket(self, socketObj, topic):
        """Subscribe a socket to a topic."""
        print("ERROR: UfmmqClient.subscribe_socket must be implemented!")

    def create_poller(self, timeout=None):
        """Create a poller for event monitoring."""
        print("ERROR: UfmmqClient.create_poller must be implemented!")
        return None

    def register_poller(self, pollerObj, flags):
        """Register a poller for event monitoring."""
        print("ERROR: UfmmqClient.register_poller must be implemented!")

def is_valid_init_args(**kwargs):
    print("ERROR: UfmmqClient.is_valid_init_args must be implemented!")
    return True

def client(**kwargs):
    return UfmmqClient(**kwargs)

def log_errors(func):
    def func_wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            print(e)
            return None
    return func_wrapper

