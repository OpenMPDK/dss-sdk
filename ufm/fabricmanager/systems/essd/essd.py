
from systems.ufmarg import UfmArg
from systems.subsystem import SubSystem

from systems.essd.essd_poller import EssdPollerArg
from systems.essd.essd_poller import EssdDrive
from systems.essd.essd_poller import essdRedFishPoller
from systems.essd.essd_poller import EssdPoller

from systems.essd.essd_monitor import EssdMonitorArg
from systems.essd.essd_monitor import essdMonitor
from systems.essd.essd_monitor import essdMonitorCallback
from systems.essd.essd_monitor import EssdMonitor


class Essd(SubSystem):
    def __init__(self, ufmArg):
        self.ufmArg = ufmArg
        self.db = self.ufmArg.db
        self.log = self.ufmArg.log
        self.pollerArg = EssdPollerArg(db=self.db, logger=self.log)
        self.monitorArg = EssdMonitorArg()
        SubSystem.__init__(self,
                           services=(EssdPoller(hostname=self.ufmArg.hostname,
                                                logger=self.log,
                                                db=self.db,
                                                poller=essdRedFishPoller,
                                                pollerArgs=self.pollerArg),
                                     EssdMonitor(hostname=self.ufmArg.hostname,
                                                 logger=self.log,
                                                 db=self.db,
                                                 monitor=essdMonitor,
                                                 monitorArgs=self.monitorArg,
                                                 monitorCallback=essdMonitorCallback)
                                     ))

        self.log.info("Init {}".format(self.__class__.__name__))

