

from systems.ufmarg import UfmArg
from systems.subsystem import SubSystem

from systems.ufm.ufm_monitor import UfmMonitor
from systems.ufm.ufm_controller import UfmController
from systems.ufm.ufm_poller import UfmPoller


class Ufm(SubSystem):
    def __init__(self, ufmArg):
        self.ufmArg = ufmArg
        self.log = self.ufmArg.log
        SubSystem.__init__(self, services=(UfmMonitor(ufmArg=self.ufmArg),
                                           UfmPoller(ufmArg=self.ufmArg),
                                           UfmController(ufmArg=self.ufmArg)
                                           ))

        self.log.info("Init {}".format(self.__class__.__name__))

