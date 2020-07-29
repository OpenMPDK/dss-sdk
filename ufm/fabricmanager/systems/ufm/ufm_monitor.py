import threading

from ufm_thread import UfmThread
from systems.ufm import ufm_constants
from systems.ufm_message import Subscriber


class UfmMonitor(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self.event = threading.Event()

        self.log = self.ufmArg.log
        self.db = self.ufmArg.db

        self.msgListner = Subscriber(event=self.event,
                                     ports=(self.ufmArg.ufmPorts),
                                     topics=('ufmmonitor',),
                                     process=self.processMonitorrMessages)
        self._running = False
        self.watch_id = None
        super(UfmMonitor, self).__init__()

    def __del__(self):
        pass
        # self.stop()

    def _ufmMonitorCallback(self, event=None):
        if not isinstance(event.events, list):
            return

        # There has been a changed in the /ufm section of the db
        pass

    def _ufmMonitor(self, cbArgs):
        """
           This function run in a thread
           Do some monitor work here
        """
        msg = {'module': 'ufm',
               'service': 'monitor',
               'status': True}

        cbArgs.publisher.send('ufmcontroller', msg)

    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))
        self.msgListner.start()

        self._running = True
        super(UfmMonitor, self).start(threadName='UfmMonitor',
                                      cb=self._ufmMonitor,
                                      cbArgs=self.ufmArg,
                                      repeatIntervalSecs=7.0)

        try:
            self.watch_id = self.db.add_watch_callback(ufm_constants.UFM_PREFIX, self._ufmMonitorCallback)
        except Exception as e:
            self.log.error('UFM: Could not set callback (key={}) (id={})'.format(ufm_constants.UFM_PREFIX,
                                                                                 str(e)))
            self.watch_id = None

        self.log.info("====> UFM: Done Configure DB key watch'er <====")

    def stop(self):
        super(UfmMonitor, self).stop()
        self.log.info("Stop {}".format(self.__class__.__name__))
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False

        if not self.db:
            self.log.error("DB is closed")
        else:
            if not self.watch_id:
                self.log.error("UFM module: invalid watch ID")
            else:
                self.db.cancel_watch(self.watch_id)

    def is_running(self):
        return self._running

    def processMonitorrMessages(self, topic, message):
        # print("Topic: {}  Message to process is: {}\n".format(topic, message))

        # this will send a Aka to publisher
        msg_ok = dict()
        msg_ok['status'] = True

        self.ufmArg.publisher.send("Aka", msg_ok)
