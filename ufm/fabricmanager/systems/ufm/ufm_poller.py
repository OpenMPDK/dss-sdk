import os
import threading

from ufm_thread import UfmThread


class UfmPoller(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self._running = False

        super(UfmPoller, self).__init__()
        self.ufmArg.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        self.ufmArg.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass


    def start(self):
        self.ufmArg.log.info("Start {}".format(self.__class__.__name__))
        self._running = True
        super(UfmPoller, self).start(threadName='UfmPoller', cb=self._poller, cbArgs=self.ufmArg, repeatIntervalSecs=6.0)


    def stop(self):
        super(UfmPoller, self).stop()
        self._running = False
        self.ufmArg.log.info("Stop {}".format(self.__class__.__name__))


    def is_running(self):
        return self._running


    def _poller(self, ufmArg):
        # Read disk space of node and write it to db
        df_struct = os.statvfs('/')
        if df_struct.f_blocks > 0:
            df_out = df_struct.f_bfree * 100 / df_struct.f_blocks
            if df_out:
                ufmArg.db.put(ufmArg.prefix + "/space_avail_percent", df_out)

        # Do more here is needed
        pass





