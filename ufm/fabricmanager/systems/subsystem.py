

class SubSystem():
    def __init__(self, services, log=None):
        self.log = log
        if self.log:
            self.log.info("SubSystem Init {}".format(self.__class__.__name__))
        self.services = services
        self._running = False

    def __del__(self):
        if self.log:
            self.log.info("SubSystem Del {}".format(self.__class__.__name__))

        self._running = False

    def start(self):
        if self.log:
            self.log.info("SubSystem Start {}".format(self.__class__.__name__))

        for service in iter(self.services):
            service.start()
            self._running = True

    def stop(self):
        if self.log:
            self.log.info("SubSystem Stop {}".format(self.__class__.__name__))

        for service in iter(self.services):
            service.stop()

        self._running = False

    def is_running(self):
        return self._running
