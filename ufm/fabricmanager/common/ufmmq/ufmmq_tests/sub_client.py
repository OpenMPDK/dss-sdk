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
# sub_client.py pair with pub_server.py
# refer to notes in pub_server.py
#
import sys
import zmq #for zmq.SUBSCRIBE
from common.ufmmq import ufmmq

port = "5556"
if len(sys.argv) > 1:
    port =  sys.argv[1]
    int(port)

if len(sys.argv) > 2:
    port1 =  sys.argv[2]
    int(port1)

try:
    mqclient = ufmmq.client(mq_type = 'zmq')
except Exception as e:
    print(e)
    sys.exit(1)

# Socket to talk to server
print("Collecting updates from weather server...")
skt = mqclient.create_socket(zmq.SUB)
mqclient.connect_socket(skt, "tcp://localhost", port)

if len(sys.argv) > 2:
    mqclient.connect_socket(skt, "tcp://localhost", port1)

# Subscribe to zipcode, default is NYC, 10001
topicfilter1 = "10001"
topicfilter2 = "10003"
mqclient.setsockopt(skt, zmq.SUBSCRIBE, topicfilter1)
mqclient.setsockopt(skt, zmq.SUBSCRIBE, topicfilter2)

# Process 5 updates
total_value = 0
for update_nbr in range (6):
    string = mqclient.receive(skt)
    topic, messagedata = string.split()
    total_value += int(messagedata)
    print(topic, messagedata)

print("Average messagedata value for topic '%s' and '%s' was %dF" % (topicfilter1, topicfilter2, total_value / update_nbr))

