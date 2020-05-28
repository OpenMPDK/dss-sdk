



class UfmArg():
    def __init__(self, db, hostname, log, uuid):
        self.db = db
        self.hostname = hostname
        self.log = log
        self.uuid = str(uuid)
        self.prefix = "/ufm/" + self.uuid

