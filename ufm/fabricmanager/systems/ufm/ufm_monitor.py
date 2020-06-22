import threading

from ufm_thread import UfmThread
from systems.ufm_message import Subscriber


class UfmMonitor(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg

        self.log = self.ufmArg.log
        self.db = self.ufmArg.db
        self.prefix = self.ufmArg.prefix

        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=(self.ufmArg.ufmPorts),
                                     topics=('monitor',))
        self._running = False
        self.watch_id = None
        super(UfmMonitor, self).__init__()
        self.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        self.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass


    def _ufmMonitorCallback(self, event=None):
        if not isinstance(event.events, list):
            return

        print("_UCB", flush=True, end='')


    def _ufmMonitor(self, cbArgs):
        """
           This function run in a thread
           Do some monitor work here
        """
        print("_UM", flush=True, end='')
        msg=dict()
        msg['status'] = True
        cbArgs.publisher.send('ufmcontroller', msg)


    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))
        self.msgListner.start()

        self._running = True
        super(UfmMonitor, self).start(threadName='UfmMonitor', cb=self._ufmMonitor, cbArgs=self.ufmArg, repeatIntervalSecs=7.0)

        try:
            self.watch_id = self.db.watch_callback(self.prefix, self._ufmMonitorCallback, previous_kv=True)
        except Exception as e:
            self.log.error('Exception could not get watch id: {}'.format(str(e)))
            self.watch_id = None


    def stop(self):
        self.log.info("Stop {}".format(self.__class__.__name__))
        self.msgListner.stop()
        self.msgListner.join()

        super(UfmMonitor, self).stop()
        self._running = False

        if not self.db:
            self.log.error("DB is closed")
        else:
            if not self.watch_id:
                self.log.error("Invalid watch ID")
            else:
                self.db.cancel_watch(self.watch_id)


    def is_running(self):
        return self._running


