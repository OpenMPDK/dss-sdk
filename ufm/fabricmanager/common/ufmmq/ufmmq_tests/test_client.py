#
#   test_server.py pair with test_client.py
#
#   Hello World client in Python
#   Connects REQ socket to tcp://localhost:5555
#   Sends "Hello" to server, expects "World" back
#
from common.ufmmq import ufmmq
import zmq #for zmq.REQ

try:
    mqclient = ufmmq.client(mq_type = 'zmq')
except Exception as e:
    print(e)
    sys.exit(1)

#  Socket to talk to server
print("Connecting to hello world server?")
skt = mqclient.create_socket(zmq.REQ)
mqclient.connect_socket(skt, "tcp://localhost", "5555")

#  Do 10 requests, waiting each time for a response
for request in range(10):
    print("Sending request %s ?" % request)
    mqclient.send(skt, "Hello")

    #  Get the reply.
    msg = mqclient.receive(skt)
    print("Received reply %s [ %s ]" % (request, msg))
