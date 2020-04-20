
class Poller:
    def __init__(self):
        self.running = False

    def start(self):
        self.running = True
        #print(f'{__name__} Started')

    def stop(self):
        self.running = False
        #print(f'{__name__} Stop')

    def is_running(self):
        return self.running

