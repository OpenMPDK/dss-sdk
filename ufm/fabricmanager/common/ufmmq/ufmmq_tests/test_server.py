#
#   test_server.py pair with test_client.py
#
#   Hello World server in Python
#   Binds REP socket to tcp://*:5555
#   Expects b"Hello" from client, replies with b"World"
#
from common.ufmmq import ufmmq
import time
import zmq #for zmq.REP

print('\nTest 1: **********Opening MQ connection**********\n')
try:
    mqclient = ufmmq.client(mq_type = 'zmq')
except Exception as e:
    print(e)
    sys.exit(1)

#context = zmq.Context()
#socket = context.socket(zmq.REP)
#socket.bind("tcp://*:5555")
skt = mqclient.create_socket(zmq.REP)
mqclient.bind_socket(skt, "tcp://*", "5555")

while True:
    #  Wait for next request from client
    #message = socket.recv()
    msg = mqclient.receive(skt)
    print("Received request: %s" % msg)

    #  Do some 'work'
    time.sleep(1)

    #  Send reply back to client
    #socket.send(b"World")
    mqclient.send(skt, "World")
