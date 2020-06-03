import os
import threading
from ufm_thread import UfmThread
from systems.switch.switch_arg import SwitchArg


class SwitchPoller(UfmThread):
    def __init__(self, swArg=None):
        self.swArg = swArg
        self.log = self.swArg.log
        self._running = False
        super(SwitchPoller, self).__init__()
        self.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        self.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass


    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))
        self._running = True
        super(SwitchPoller, self).start(threadName='SwitchPoller', cb=self.pollerX, cbArgs=self.swArg, repeatIntervalSecs=1.0)


    def stop(self):
        super(SwitchPoller, self).stop()
        self._running = False
        self.log.info("Stop {}".format(self.__class__.__name__))


    def is_running(self):
        return self._running

    def pollerX(self, ufmArg):
        print("_SP_", flush=True, end='')
        # Do more here if needed
        pass

