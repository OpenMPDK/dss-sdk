



class UfmArg():
    def __init__(self, db, hostname, log, uuid, ufmMainEvent, publisher = None):
        self.db = db
        self.hostname = hostname
        self.log = log
        self.uuid = str(uuid)
        self.prefix = "/ufm/" + self.uuid
        self.mainEvent = ufmMainEvent
        self.publisher = publisher

