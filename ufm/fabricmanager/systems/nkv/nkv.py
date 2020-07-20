

from common.system.controller import Controller
from common.system.collector import Collector
# from common.system.monitor import Monitor
from common.system.poller import Poller

from systems.nkv.nkv_monitor import NkvMonitor
from common.ufmlog import ufmlog


class Nkv():
    def __init__(self, ufmArg=None, hostname=None, db=None):
        self.ufmArg = ufmArg
        self.hostname = hostname
        self.log = ufmlog.log(module=__name__, mask=ufmlog.UFM_NKV)
        self.log.log_detail_on()
        self.log.log_debug_on()
        self.db = db
        self.__running = False
        self.services = (
            Collector(),
            Poller(),
            NkvMonitor(ufmArg=ufmArg, hostname=hostname, logger=self.log, db=db),
            Controller()
        )
        self.log.info('===> Init Nkv\'s sub system <===')

    def start(self):
        self.log.info('===> Start all Nkv\'s sub system services <===')

        for service in self.services:
            if not service.is_running():
                service.start()
                self.__running = True

    def stop(self):
        self.log.info('===> Stop all Nkv\'s sub system services <===')
        for service in self.services:
            service.stop()
        self.__running = False

    def is_running(self):
        return self.__running
