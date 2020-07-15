

class UfmArg():
    def __init__(self):
        self.db = None
        self.hostname = None
        self.log = None
        self.uuid = ""
        self.prefix = "/empty/"
        self.mainEvent = None
        self.publisher = None

    def set(self, db, hostname, log, uuid, ufmMainEvent, publisher=None):
        self.db = db
        self.hostname = hostname
        self.log = log
        self.uuid = str(uuid)
        self.prefix = "/ufm/" + self.uuid
        self.mainEvent = ufmMainEvent
        self.publisher = publisher
