import threading

from ufm_thread import UfmThread

from systems import port_def
from systems.ufm_message import Subscriber


class UfmController(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=(self.ufmArg.ufmPorts),
                                     topics=('ufmcontroller',),
                                     process=self.processControllerMessages)
        super(UfmController, self).__init__()
        self.ufmArg.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        self.ufmArg.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass


    def start(self):
        self.ufmArg.log.info("Start {}".format(self.__class__.__name__))
        self.msgListner.start()

        self._running = True
        super(UfmController, self).start(threadName='UfmController', cb=self.controller, cbArgs=self.ufmArg, repeatIntervalSecs=3.0)


    def stop(self):
        super(UfmController, self).stop()
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False
        self.ufmArg.log.info("Stop {}".format(self.__class__.__name__))


    def is_running(self):
        return self._running


    def controller(self, ufmArg):
        pass


    def processControllerMessages(self, topic, message):
        print("\nTopic: {}\nMessage to process is: {}\n".format(topic, message) )

        print("Recv msg: OK")

        # this will send a Aka to publisher
        msg_ok = dict()
        msg_ok['status'] = True

        self.ufmArg.publisher.send("Aka", msg_ok)
        print("Send OK")

