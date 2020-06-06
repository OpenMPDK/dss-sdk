
from ufm_thread import UfmThread
from systems.switch.switch_mellanox.switch_mellanox_client import SwitchMellanoxClient

class SwitchPoller(UfmThread):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = self.swArg.log
        self._running = False
        self.client = None

        super(SwitchPoller, self).__init__()
        self.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        self.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass


    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))

        if self.swArg.sw_type.lower() == 'mellanox':
            self.client = SwitchMellanoxClient(self.swArg)
        else:
            raise Exception('Invalid switch type provided, {} is not valid'.format(swArg.sw_type))

        self._running = True
        super(SwitchPoller, self).start(threadName='SwitchPoller', cb=self._pollerX, cbArgs=self.swArg, repeatIntervalSecs=10.0)


    def stop(self):
        super(SwitchPoller, self).stop()
        self._running = False
        self.log.info("Stop {}".format(self.__class__.__name__))


    def is_running(self):
        return self._running


    def _pollerX(self, ufmArg):
        print("_SP_", flush=True, end='')
        self.client.poll_to_db()

