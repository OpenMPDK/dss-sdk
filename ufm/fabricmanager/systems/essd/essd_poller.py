from datetime import datetime

import threading
import json

from ufm_thread import UfmThread
from systems.ufm_message import Subscriber

from systems.essd import essd_constants
from systems.essd.essd_drive import EssdDrive


class EssdPoller(UfmThread):
    """
        Description:
           This thread is pollon Essd's and add the Essd'd
           metadata to DB
    """
    def __init__(self, ufmArg=None, essdArg=None, pollerArgs=None):
        self.ufmArg = ufmArg
        self.essdArg = essdArg
        self.pollerArgs = pollerArgs

        self._running = False
        self.hostname = ufmArg.hostname
        self.log = ufmArg.log
        self.db = ufmArg.db

        self.ports = self.ufmArg.ufmPorts
        self.ports.append(self.ufmArg.essdConfig['messageQueuePort'])

        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=self.ports,
                                     topics=('poller',))
        super(EssdPoller, self).__init__()
        self.log.info('===> Init Essd <===')

    def __del__(self):
        if self._running:
            self.stop()
        self.log.info('===> Delete Essd <===')

    def start(self):
        self.msgListner.start()

        # Force the scan of Essd at startup
        self.pollerArgs.initialScan = False
        self.pollerArgs.publisher = self.essdArg.publisher

        self._running = True
        super(EssdPoller, self).start(threadName='EssdPoller',
                                      cb=self.essdRedFishPoller,
                                      cbArgs=self.pollerArgs,
                                      repeatIntervalSecs=15.0)
        self.log.info('===> Start Essd <===')

        msg = {'module': 'essd',
               'service': 'poller',
               'running': True}

        self.essdArg.publisher.send('status', msg)

    def stop(self):
        super(EssdPoller, self).stop()
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False
        self.log.info('===> Stop Essd <===')

        msg = {'module': 'essd',
               'service': 'poller',
               'running': False}

        self.essdArg.publisher.send('status', msg)

    def is_running(self):
        return self._running

    def scanOneEssd(self, essdUrl=None, db=None, log=None):
        try:
            essd = EssdDrive(url=essdUrl,
                             username=None,
                             password=None,
                             log=log)
        except Exception as e:
            log.error("Failed to connect to essd {}".format(essdUrl))
            log.exception(e)
            return False

        # Update uuid's latest upTime
        essd.updateUuid(db)
        # Add lookup entry
        essd.checkAddLookupEntry(db)

        # Read all the RedFish data from essds
        essd.readEssdData(db)

        return True

    def essdRedFishPoller(self, cbArgs):
        # Remove dead uuid from DB, less often
        if cbArgs.updateEssdUrls:
            pass
            # TODO - Add this clean up in a new ticket
            # EssdDrive.removeUuidOlderThan(cbArgs.db, 1200)

        # Read essd url's from DB
        if not cbArgs.essdSystems or cbArgs.updateEssdUrls:
            try:
                tmpString, _ = cbArgs.db.get(essd_constants.ESSDURLS_KEY)
                cbArgs.essdSystems = json.loads(tmpString.decode('utf-8'))
                cbArgs.updateEssdUrls = False
            except Exception as e:
                cbArgs.log.error("Failed to read essd Urls from db {}"
                                 .format(tmpString))
                cbArgs.log.exception(e)

        if not cbArgs.essdSystems:
            return

        # Initial scan of all essd's
        if not cbArgs.initialScan:
            cbArgs.scanSuccess = True

            startTime = datetime.now()
            for essdUrl in cbArgs.essdSystems:
                rc = self.scanOneEssd(essdUrl=essdUrl,
                                      db=cbArgs.db,
                                      log=cbArgs.log)
                if not rc:
                    cbArgs.scanSuccess = False
                    cbArgs.log.warning("Fail to scan all essd's")

            cbArgs.initialScan = True
            stopTime = datetime.now()
            diffTime = int((stopTime - startTime).seconds)
            numberOfEssds = len(cbArgs.essdSystems)

            msg = {'module': 'essd',
                   'service': 'poller',
                   'scanAllEssds': True,
                   'scanSuccess': cbArgs.scanSuccess,
                   'totalScanTime': diffTime,
                   'numberOfESSDs': numberOfEssds}

            cbArgs.publisher.send('ufmcontroller', msg)
            return

        cbArgs.essdCounter = cbArgs.essdCounter + 1
        essdToScan = cbArgs.essdCounter % len(cbArgs.essdSystems)
        essdUrl = cbArgs.essdSystems[essdToScan]

        self.scanOneEssd(essdUrl=essdUrl, db=cbArgs.db, log=cbArgs.log)
