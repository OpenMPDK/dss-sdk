
import os
import os.path
import sys
import threading
import json
import signal
import time
import datetime

from ufm_thread import UfmThread

from systems import port_def
from systems.ufm_message import Subscriber

from systems.essd import essd_constants
from systems.essd.essd_arg import EssdPollerArg
from systems.essd.essd_drive import EssdDrive



class EssdPoller(UfmThread):
    """
        Description:
           This thread is pollon Essd's and add the Essd'd
           metadata to DB
    """
    def __init__(self, ufmArg=None, essdArg=None, pollerArgs=None):
        self.essdArg = essdArg
        self._running = False
        self.hostname = ufmArg.hostname
        self.log = ufmArg.log
        self.db = ufmArg.db
        self.pollerArgs = pollerArgs

        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=(port_def.ESSD,
                                            port_def.UFM),
                                     topics=('poller',))
        super(EssdPoller, self).__init__()
        self.log.info('===> Init Essd <===')


    def __del__(self):
        if self._running:
            self.stop()
        self.log.info('===> Delete Essd <===')


    def start(self):
        self.log.info('===> Start Essd <===')
        self.msgListner.start()

        self._running = True
        super(EssdPoller, self).start(threadName='EssdPoller', cb=self.essdRedFishPoller, cbArgs=self.pollerArgs, repeatIntervalSecs=30.0)
        self.essdArg.publisher.send('main', "{ essd_poller_running: True }")


    def stop(self):
        super(EssdPoller, self).stop()
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False
        self.log.info('===> Stop Essd <===')
        self.essdArg.publisher.send('main', "{ essd_poller_running: False }")


    def is_running(self):
        return self._running


    def essdRedFishPoller(self, cbArgs):
        # Remove dead uuid from DB, less often
        if cbArgs.updateEssdUrls:
            essd.removeUuidOlderThan(cbArgs.db, 1200)

        cbArgs.essdCounter = cbArgs.essdCounter + 1

        # Read essd url's from DB
        if not cbArgs.essdSystems or cbArgs.updateEssdUrls:
            try:
                tmpString, _ = cbArgs.db.get( essd_constants.ESSDURLS_KEY )
                cbArgs.essdSystems = json.loads(tmpString.decode('utf-8'))
                cbArgs.updateEssdUrls = False
            except:
                cbArgs.log.error("Failed to read essd Urls from db {}".format(tmpString))

        essdToScan = cbArgs.essdCounter % len(cbArgs.essdSystems)
        essdUrl = cbArgs.essdSystems[essdToScan]

        try:
            essd = EssdDrive(url=essdUrl, username=None, password=None, log=cbArgs.log)
        except Exception as e:
            cbArgs.log.error("Failed to connect to essd {}".format(essdUrl))
            cbArgs.log.exception(e)
            return

        # Update uuid's latest upTime
        essd.updateUuid(cbArgs.db)
        # Add lookup entry
        essd.checkAddLookupEntry(cbArgs.db)

        # Read all the RedFish data from essds
        essd.readEssdData(cbArgs.db)


