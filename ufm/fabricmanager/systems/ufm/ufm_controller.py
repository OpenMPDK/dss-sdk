import threading

from ufm_thread import UfmThread


class UfmController(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg

        super(UfmController, self).__init__()
        self.ufmArg.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        self.ufmArg.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass


    def start(self):
        self.ufmArg.log.info("Start {}".format(self.__class__.__name__))
        self._running = True
        super(UfmController, self).start(threadName='UfmController', cb=self.controller, cbArgs=self.ufmArg, repeatIntervalSecs=3.0)


    def stop(self):
        super(UfmController, self).stop()
        self._running = False
        self.ufmArg.log.info("Stop {}".format(self.__class__.__name__))


    def is_running(self):
        return self._running


    def controller(self, ufmArg):
        # This is the function that does the work
        pass








