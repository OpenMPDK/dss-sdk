
from common.system.controller import Controller
from common.system.monitor import Monitor
from common.system.collector import Collector
from common.system.poller import Poller

from common.ufmlog import ufmlog

from systems.essd.essd_poller import EssdPollerArg
from systems.essd.essd_poller import EssdDrive
from systems.essd.essd_poller import essdRedFishPoller
from systems.essd.essd_poller import EssdPoller


from systems.essd.essd_monitor import EssdMonitorArg
from systems.essd.essd_monitor import essdMonitor
from systems.essd.essd_monitor import essdMonitorCallback
from systems.essd.essd_monitor import EssdMonitor


class Essd():
    def __init__(self, hostname=None, db=None):
        self.hostname = hostname
        self.db = db
        self.log = ufmlog.log(module=__name__, mask=ufmlog.UFM_ESSD)
        self.log.log_detail_on()
        self.log.log_debug_on()
        self.__running = False
        self.pollerArg = EssdPollerArg(db=self.db, logger=self.log)
        self.monitorArg = EssdMonitorArg()
        self.services = (
            Collector(),
            EssdPoller(hostname=hostname,
                       logger=self.log,
                       db=self.db,
                       poller=essdRedFishPoller,
                       pollerArgs=self.pollerArg),
            EssdMonitor(hostname=hostname,
                        logger=self.log,
                        db=self.db,
                        monitor=essdMonitor,
                        monitorArgs=self.monitorArg,
                        monitorCallback=essdMonitorCallback),
            Controller()
        )
        self.log.info('===> Init Essd\'s sub system <===')


    def start(self):
        self.log.info('===> Start all Essd\'s sub system services <===')
        for service in self.services:
            if not service.is_running():
                service.start()
                self.__running = True


    def stop(self):
        self.log.info('===> Stop all Essd\'s sub system services <===')
        for service in self.services:
            service.stop()
        self.__running = False


    def is_running(self):
        return self.__running

