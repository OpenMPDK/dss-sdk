
import os
import os.path
import sys
import threading
import json
import signal
import time
import datetime

from ufm_thread import UfmThread
from systems.essd.essd_drive import EssdDrive


class EssdPollerArg():
    """
       Variables for the thread function
    """
    def __init__(self, db, logger):
        self.db = db
        self.logger = logger
        self.essdSystems = list()
        self.essdCounter = -1


def essdRedFishPoller(cbArgs):
    """
       This function get called from a thread, every x sec, see ufm_thread.py
    """
    cbArgs.essdCounter = cbArgs.essdCounter + 1

    # Read essd url's from DB
    if not cbArgs.essdSystems or (cbArgs.essdCounter % 13) == 0:
        try:
            tmpString = cbArgs.db.get_key_value("/essd/essdurls").decode('utf-8')
            cbArgs.essdSystems = json.loads(tmpString)
        except:
            cbArgs.logger.error("Failed to read essd Urls from db {}".format(tmpString))

    essdToScan = cbArgs.essdCounter % len(cbArgs.essdSystems)
    essdUrl = cbArgs.essdSystems[essdToScan]

    try:
        essd = EssdDrive(url=essdUrl, username=None, password=None, logger=cbArgs.logger)
    except:
        cbArgs.logger.error("Failed to connect to essd {}".format(essdUrl))
        return

    # Update uuid's latest upTime
    essd.updateUuid(cbArgs.db)

    # Read all the RedFish data from essds
    essd.readEssdData(cbArgs.db)

    # Remove dead uuid from DB, less often
    if (cbArgs.essdCounter % 17) == 0:
        essd.removeUuidOlderThan(cbArgs.db, 1200)


class EssdPoller(UfmThread):
    """
        Description:
           This thread is pollon Essd's and add the Essd'd
           metadata to DB
    """
    def __init__(self, hostname=None, logger=None, db=None, poller=None, pollerArgs=None):
        self.hostname = hostname
        self.logger = logger
        self.db = db
        self.poller = poller
        self.pollerArgs = pollerArgs
        self.running = False
        super(EssdPoller, self).__init__()
        self.logger.info('===> Init Essd <===')


    def __del__(self):
        self.stop()
        self.logger.info('===> Delete Essd <===')


    def start(self):
        self.logger.info('===> Start Essd <===')
        self.running = True
        super(EssdPoller, self).start(threadName='EssdPoller', cb=self.poller, cbArgs=self.pollerArgs, repeatIntervalSecs=30.0)


    def stop(self):
        super(EssdPoller, self).stop()
        self.running = False
        self.logger.info('===> Stop Essd <===')


    def is_running(self):
        return self.running


