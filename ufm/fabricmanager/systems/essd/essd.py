
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


tmpSystems=["http://172.22.4.58:32768", "http://172.22.4.58:32790", "http://172.22.4.58:32789",
            "http://172.22.4.58:32783", "http://172.22.4.58:32780", "http://172.22.4.58:32778",
            "http://172.22.4.58:32776", "http://172.22.4.58:32791", "http://172.22.4.58:32792",
            "http://172.22.4.58:32782", "http://172.22.4.58:32785", "http://172.22.4.58:32779",
            "http://172.22.4.58:32775", "http://172.22.4.58:32777", "http://172.22.4.58:32772",
            "http://172.22.4.58:32781", "http://172.22.4.58:32770", "http://172.22.4.58:32773",
            "http://172.22.4.58:32769", "http://172.22.4.58:32788", "http://172.22.4.58:32786",
            "http://172.22.4.58:32784", "http://172.22.4.58:32787", "http://172.22.4.123:32775",
            "http://172.22.4.123:32781", "http://172.22.4.123:32782", "http://172.22.4.123:32786",
            "http://172.22.4.123:32788", "http://172.22.4.123:32773", "http://172.22.4.123:32779",
            "http://172.22.4.123:32776", "http://172.22.4.123:32780", "http://172.22.4.123:32791",
            "http://172.22.4.123:32768", "http://172.22.4.123:32771", "http://172.22.4.123:32777",
            "http://172.22.4.123:32787", "http://172.22.4.123:32778", "http://172.22.4.123:32792",
            "http://172.22.4.123:32783", "http://172.22.4.123:32769", "http//172.22.4.123:32772",
            "http://172.22.4.123:32774", "http://172.22.4.123:32790", "http://172.22.4.123:32784",
            "http://172.22.4.123:32785", "http://172.22.4.60:32772", "http://172.22.4.60:32776",
            "http://172.22.4.60:32775", "http://172.22.4.60:32788", "http://172.22.4.60:32785",
            "http://172.22.4.60:32793", "http://172.22.4.60:32790", "http://172.22.4.60:32774",
            "http://172.22.4.60:32795", "http://172.22.4.60:32779", "http://172.22.4.60:32777",
            "http://172.22.4.60:32780", "http://172.22.4.60:32784", "http://172.22.4.60:32789",
            "http://172.22.4.60:32787", "http://172.22.4.60:32792", "http://172.22.4.60:32796",
            "http://172.22.4.60:32781", "http://172.22.4.60:32773", "http://172.22.4.60:32778",
            "http://172.22.4.60:32783", "http://172.22.4.60:32786", "http://172.22.4.60:32782",
            "http://172.22.4.61:32774", "http://172.22.4.61:32789", "http://172.22.4.61:32769",
            "http://172.22.4.61:32790", "http://172.22.4.61:32785", "http://172.22.4.61:32779",
            "http://172.22.4.61:32781", "http://172.22.4.61:32775", "http://172.22.4.61:32780",
            "http://172.22.4.61:32770", "http://172.22.4.61:32778", "http://172.22.4.61:32786",
            "http://172.22.4.61:32787", "http://172.22.4.61:32782", "http://172.22.4.61:32783",
            "http://172.22.4.61:32791", "http://172.22.4.61:32792", "http://172.22.4.61:32784",
            "http://172.22.4.61:32788", "http://172.22.4.61:32776", "http://172.22.4.61:32777",
            "http://172.22.4.61:32771", "http://172.22.4.61:32773"]


class Essd():
    def __init__(self, hostname=None, db=None):
        self.hostname = hostname
        self.db = db
        self.log = ufmlog.log(module=__name__, mask=ufmlog.UFM_ESSD)
        self.log.log_detail_on()
        self.log.log_debug_on()
        self.__running = False
        self.pollerArg = EssdPollerArg(db=self.db, logger=self.log, systems=tmpSystems )
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

