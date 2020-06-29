
import time
import threading
import zmq
import json


class Subscriber(threading.Thread):
    def __init__(self, event=None, ports=None, topics=None, process=None):
        self.event = event
        self.ports = ports
        self.topicfilter = topics
        if not process:
            self.process = self.debug
        else:
            self.process = process
        super(Subscriber, self).__init__()
        self.context = zmq.Context()

    def __del__(self):
        self.event.set()
        self.context.destroy()

    def run(self):
        socket = self.context.socket(zmq.SUB)

        for port in self.ports:
            socket.connect("tcp://localhost:{}".format(port))

        for topic in self.topicfilter:
            socket.setsockopt_string(zmq.SUBSCRIBE, topic)

        # self.event.clear()
        while not self.event.is_set():
            try:
                string = socket.recv_string(flags=zmq.NOBLOCK)
            except KeyboardInterrupt:
                break
            except Exception:
                time.sleep(0.5)
                continue

            topic, message = string.split('=')

            self.process(topic, message)

        socket.close()

    def stop(self):
        self.event.set()

    def debug(self, topic, message):
        print("{} {}".format(topic, message))


class Publisher():
    def __init__(self, port=None):
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        self.socket.bind("tcp://*:{}".format(self.port))

    def __del__(self):
        if self.socket:
            self.socket.close()

        self.context.destroy()

    def send(self, topic=None, jsonMessage=None):
        if not topic:
            return

        if not jsonMessage:
            return

        if type(jsonMessage) is dict:
            tmpString = json.dumps(jsonMessage)
            self.socket.send_string("{}={}".format(topic, tmpString))
