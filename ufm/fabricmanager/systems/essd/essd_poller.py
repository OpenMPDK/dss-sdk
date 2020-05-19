
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
    def __init__(self, db, logger, systems):
        self.db = db
        self.logger = logger
        self.essdSystems = systems
        self.essdToScan = 0


def rightRotate(lists, num):
    output_list = []

    # Will add values from n to the new list
    for item in range(len(lists) - num, len(lists)):
        output_list.append(lists[item])

    # Will add the values before
    # n to the end of new list
    for item in range(0, len(lists) - num):
        output_list.append(lists[item])

    return output_list


def essdRedFishPoller(cbArgs):
    """
       This function get called from a thread, every x sec, see ufm_thread.py
    """
    cbArgs.essdSystems = rightRotate(cbArgs.essdSystems, 1)

    e = cbArgs.essdSystems[0]
    cbArgs.logger.debug("In Redfish poller {}".format(e))

    try:
        essd = EssdDrive(url=e, username=None, password=None, logger=cbArgs.logger)
    except:
        cbArgs.logger.error("ERR: Failed to connect to essd {}".format(e))
        return

    # Update uuid's latest upTime
    essd.updateUuid(cbArgs.db)

    # Read all the RedFish data from essds
    essd.readEssdData(cbArgs.db)

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


