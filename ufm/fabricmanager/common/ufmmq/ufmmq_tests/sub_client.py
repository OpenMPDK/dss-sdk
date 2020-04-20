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

