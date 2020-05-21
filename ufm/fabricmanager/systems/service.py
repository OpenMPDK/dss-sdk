


class Service:
    def __init__(self):
        self.__running = False


    def start(self):
        self.__running = True


    def stop(self):
        self.__running = False


    def is_running(self):
        return self.__running

