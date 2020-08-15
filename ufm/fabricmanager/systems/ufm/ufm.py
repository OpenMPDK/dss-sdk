
from systems.subsystem import SubSystem

from systems.ufm.ufm_monitor import UfmMonitor
from systems.ufm.ufm_controller import UfmController
from systems.ufm.ufm_poller import UfmPoller


class Ufm(SubSystem):
    def __init__(self, ufmArg):
        ufmArg.log.info("Init {}".format(self.__class__.__name__))
        SubSystem.__init__(self,
                           services=(UfmMonitor(ufmArg=ufmArg),
                                     UfmPoller(ufmArg=ufmArg),
                                     UfmController(ufmArg=ufmArg)),
                           log=ufmArg.log)
