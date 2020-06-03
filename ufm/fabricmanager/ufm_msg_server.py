import time
import threading
import zmq

from systems import port_def


class UfmMessageServer(threading.Thread):
    def __init__(self, ufmMainEvent, args=()):
        super(UfmMessageServer, self).__init__()
        self.ufmMainEvent = ufmMainEvent
        self.args = args

        self.socket = zmq.Context().socket(zmq.PUB)
        self.socket.bind("tcp://*:{}".format(port_def.UFM_MAIN))


    def __del__(self):
        pass


    def run(self):
        print("Zmq version {}".format(zmq.zmq_version()))

        steps = self.args[0] # number of step

        message=None
        while not self.ufmMainEvent.is_set():
            try:
                message = self.socket.recv(flags=zmq.NOBLOCK)
                # socket.send(b"World")
            except Exception as e:
                message=None
                time.sleep(1)

            if not message:
                continue

            print("Received request: {}".format(message) )

