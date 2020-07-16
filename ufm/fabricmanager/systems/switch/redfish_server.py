
import time
import zmq
import threading

class Rfserver(threading.Thread):
    def __init__(self, event=None, port=None, process=None):
        self.event = event
        self.port = port
        self.process = process
        self.context = zmq.Context()
        super(Rfserver, self).__init__()

    def __del__(self):
        self.event.set()
        self.context.destroy()

    def run(self):
        socket = self.context.socket(zmq.REP)
        socket.bind("tcp://*:{}".format(self.port))

        self.event.clear()
        while not self.event.is_set():
            try:
                jsonRequest = socket.recv_json(flags=zmq.NOBLOCK)
                jsonResponse = self.process(jsonRequest)

                socket.send_json(jsonResponse)
            except KeyboardInterrupt:
                break
            except Exception:
                time.sleep(1)
                continue

        socket.close()

    def stop(self):
        self.event.set()


