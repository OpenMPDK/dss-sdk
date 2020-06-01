

class SubSystem():
    def __init__(self, services):
        print("Subsystem Init {}".format(self.__class__.__name__))
        self.services = services
        self._running = False


    def __del__(self):
        print("SubSystem Del {}".format(self.__class__.__name__))
        self._running = False


    def start(self):
        print("SubSystem Start {}".format(self.__class__.__name__))
        for service in iter(self.services):
            service.start()
            self._running = True


    def stop(self):
        print("SubSystem Stop {}".format(self.__class__.__name__))
        for service in iter(self.services):
            service.stop()

        self._running = False


    def is_running(self):
        return self._running


