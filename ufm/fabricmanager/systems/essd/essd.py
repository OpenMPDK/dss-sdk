
from common.system.controller import Controller
from common.system.monitor import Monitor
from common.system.collector import Collector
from common.system.poller import Poller

from common.ufmlog import ufmlog


class Essd():
    def __init__(self):
        self.__running = False
        self.services = (Collector(), Poller(), Monitor(), Controller())
        self.log = ufmlog.log(module=__name__, mask=ufmlog.UFM_NKV)
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
