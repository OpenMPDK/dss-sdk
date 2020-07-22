
from common.system.controller import Controller
from common.system.monitor import Monitor
from common.system.collector import Collector
from common.system.poller import Poller

from common.ufmlog import ufmlog
from systems.switch.switch_controller import SwitchController

class Switch():
    def __init__(self, sw_type='mellanox', ip_address=None, db=None, mq=None):
        self.ip_address = ip_address
        self.db = db
        self.mq = mq
        self.__running = False
        self.log = ufmlog.log(module=__name__, mask=ufmlog.UFM_SWITCH)
        self.log.log_detail_on()
        self.log.log_debug_on()
        self.services = (
	    Collector(),
            Poller(),
	    Monitor(),
	    SwitchController(sw_type=sw_type, ip_address=ip_address,
                             log=self.log, db=db, mq=mq)
	)
        self.log.info('===> Init Switch\'s sub system <===')

    def start(self):
        self.log.info('===> Start all Switch\'s sub system services <===')
        for service in self.services:
            if not service.is_running():
                service.start()
                self.__running = True

    def stop(self):
        self.log.info('===> Stop all Switch\'s sub system services <===')
        for service in self.services:
            service.stop()
        self.__running = False

    def is_running(self):
        return self.__running

    def close(self):
        pass


