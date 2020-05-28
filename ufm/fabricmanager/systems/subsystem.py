

class SubSystem():
    def __init__(self, services):
        print("SubSystem Init")
        self.services = services
        self._running = False


    def __del__(self):
        print("SubSystem Del")
        self._running = False


    def start(self):
        print("SubSystem Start")
        for service in iter(self.services):
            if not service.is_running():
                service.start()
                self._running = True


    def stop(self):
        print("SubSystem Stop")
        for service in iter(self.services):
            service.stop()

        self._running = False


    def is_running(self):
        return self._running


