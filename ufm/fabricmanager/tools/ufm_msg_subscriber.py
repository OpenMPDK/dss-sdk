import zmq
import platform
import socket
import time
import sys
import argparse

def subscribe(ip, port):
    connect_str = "tcp://" + ip + ":" + port
    fqdn = socket.getfqdn(ip)
    print("Subscribing to ZMQ publisher %s from host %s: " % (fqdn, platform.node()))
    ctx = zmq.Context()
    sock = ctx.socket(zmq.SUB)
    sock.setsockopt_string(zmq.SUBSCRIBE,'')
    sock.connect(connect_str)    # FabricManager01

    while(True):
        message = sock.recv_string()
        if (len(message) != 0):
            print(message)
            time.sleep(1)
        else:
            break

def main(argv):
    host_help_msg = "Hostname or IP Address of host running Zero MQ publisher"
    port_help_msg = "Zero MQ publisher port number"
    parser = argparse.ArgumentParser(
                prog='zeromq_subscribe.py', description='Arguments to Zero MQ Subscription', usage='%(prog)s --host <' + host_help_msg + '> [ --port <' + port_help_msg + '> ]')
    parser.add_argument("--host", help = host_help_msg,
                dest="host", default="localhost", required=True)
    parser.add_argument("--port", help = port_help_msg,
                dest="port", default="6001")
    args = parser.parse_args()

    subscribe(socket.gethostbyname(args.host), args.port)

if __name__ == "__main__":
    main(sys.argv[1:])
