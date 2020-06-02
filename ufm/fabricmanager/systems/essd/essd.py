
from systems.ufmarg import UfmArg
from systems.subsystem import SubSystem

from systems.essd.essd_poller import EssdPollerArg
from systems.essd.essd_poller import EssdDrive
from systems.essd.essd_poller import EssdPoller

from systems.essd.essd_monitor import EssdMonitorArg
from systems.essd.essd_monitor import essdMonitorCallback
from systems.essd.essd_monitor import EssdMonitor


KEY_ESSDURLS="/essd/essdurls"


class EssdArg():
    def __init__(self):
        self.updateEssdUrls = False


class Essd(SubSystem):
    def __init__(self, ufmArg):
        self.ufmArg = ufmArg
        self.essdArg = EssdArg()
        self.pollerArg = EssdPollerArg(db=self.ufmArg.db, log=self.ufmArg.log)
        self.monitorArg = EssdMonitorArg()
        SubSystem.__init__(self,
                           services=(EssdPoller(ufmArg=self.ufmArg,
                                                essdArg=self.essdArg,
                                                pollerArgs=self.pollerArg),
                                     EssdMonitor(ufmArg=self.ufmArg,
                                                 essdArg=self.essdArg,
                                                 monitorArgs=self.monitorArg,
                                                 monitorCallback=essdMonitorCallback)
                                     ))

        self.ufmArg.log.info("Init {}".format(self.__class__.__name__))

