from systems.subsystem import SubSystem

from systems.nkv.nkv_monitor import NkvMonitor


class Nkv(SubSystem):
    def __init__(self, ufmArg):
        ufmArg.log.info("Init {}".format(self.__class__.__name__))

        SubSystem.__init__(self,
                           services=(NkvMonitor(ufmArg=ufmArg),),
                           log=ufmArg.log)
