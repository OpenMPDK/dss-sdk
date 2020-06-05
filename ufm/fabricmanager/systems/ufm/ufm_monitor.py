import threading

from ufm_thread import UfmThread


class UfmMonitor(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg

        self.log = self.ufmArg.log
        self.db = self.ufmArg.db
        self.prefix = self.ufmArg.prefix

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


    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))
        self._running = True
        super(UfmMonitor, self).start(threadName='UfmMonitor', cb=self._ufmMonitor, cbArgs=self.ufmArg, repeatIntervalSecs=7.0)

        try:
            self.watch_id = None # self.db.watch_callback(self.prefix, self._ufmMonitorCallback, previous_kv=True)

            if not self.watch_id:
                self.log.error("=====> watch_callback returned None")

        except Exception as e:
            self.log.error('Exception could not get watch id: {}'.format(str(e)))
            self.watch_id = None


    def stop(self):
        self.log.info("Stop {}".format(self.__class__.__name__))
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


