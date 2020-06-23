

class EssdPollerArg():
    """
       Variables for the thread function
    """
    def __init__(self, db, log):
        self.db = db
        self.log = log
        self.essdSystems = list()
        self.essdCounter = -1
        self.updateEssdUrls = False
