#
# Test for publisher/subscriber pattern
#
# pub_server.py pair with sub_client.py
#
# Publisher publish the zip code and its temperature
# Subscriber subscribe to certain zip codes and calculate its average.
#

import random
import sys
import time
import zmq #for zmq.PUB
from common.ufmmq import ufmmq

port = "5556"
if len(sys.argv) > 1:
    port =  sys.argv[1]
    int(port)

try:
    mqclient = ufmmq.client(mq_type = 'zmq')
except Exception as e:
    print(e)
    sys.exit(1)

skt = mqclient.create_socket(zmq.PUB)
mqclient.bind_socket(skt, "tcp://*", port)

while True:
    topic = random.randrange(9999,10005)
    messagedata = random.randrange(1,215) - 80
    print("%d %d" % (topic, messagedata))
    mqclient.send(skt, "%d %d" % (topic, messagedata))
    time.sleep(1)
