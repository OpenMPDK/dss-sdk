
from systems.subsystem import SubSystem
from systems.switch.switch_arg import SwitchArg
from systems.switch.switch_poller import SwitchPoller
from systems.switch.switch_controller import SwitchController

class EthSwitch(SubSystem):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = self.swArg.log
        SubSystem.__init__(self, services=(SwitchPoller(swArg=self.swArg),
                                           SwitchController(swArg=self.swArg)
                                           ))

        self.log.info("Init {}".format(self.__class__.__name__))


