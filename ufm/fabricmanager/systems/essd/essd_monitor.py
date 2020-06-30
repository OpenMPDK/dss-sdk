import threading

from ufm_thread import UfmThread
from systems.ufm_message import Subscriber
from systems.essd import essd_constants


class EssdMonitorArg():
    """
       Variables for the thread function and the trigger function
    """
    def __init__(self):
        self.first = False
        self.n = 42


def essdMonitorCallback(ufmArg=None, essdArg=None, event=None):
    """
       This function is call for every update in dB
    """
    eventKeyList = event.key.decode().split('/')

    if essd_constants.ESSDURLS_KEY in eventKeyList:
        # Signal to the poller to re-scan the essd urls
        essdArg.updateEssdUrls = True


class EssdMonitor(UfmThread):
    """
       Description:
          This thread is monitor Essd's and add the Essd'd
          metadata to DB
    """
    def __init__(self,
                 ufmArg=None,
                 essdArg=None,
                 monitorArgs=None,
                 monitorCallback=None):
        self.ufmArg = ufmArg
        self.essdArg = essdArg
        self.log = ufmArg.log
        self.db = ufmArg.db
        self.monitorArgs = monitorArgs
        self.monitorCallback = monitorCallback

        self.running = False
        self.watch_id = None
        self.essdUrlId = None

        self.ports = self.ufmArg.ufmPorts
        self.ports.append(self.ufmArg.essdConfig['messageQueuePort'])

        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=self.ports,
                                     topics=('monitor',))

        super(EssdMonitor, self).__init__()
        self.log.info('===> Init Monitor <===')

    def __del__(self):
        if self.running:
            self.stop()
        self.log.info('===> Delete Essd Monitor <===')

    def _watchEssdKeyCallBack(self, event):
        if not isinstance(event.events, list):
            return

        if self.monitorCallback:
            self.monitorCallback(self.ufmArg, self.essdArg, event)

    def start(self):
        self.msgListner.start()

        self.running = True
        super(EssdMonitor, self).start(threadName='EssdMonitor',
                                       cb=self._essdMonitor,
                                       cbArgs=self.monitorArgs,
                                       repeatIntervalSecs=2.0)

        msg = {'module': 'essd',
               'service': 'monitor',
               'running': True}

        self.essdArg.publisher.send('ufmmonitor', msg)

        try:
            self.watch_id = self.db.watch_callback(essd_constants.ESSD_KEY,
                                                   self._watchEssdKeyCallBack,
                                                   previous_kv=True)
        except Exception as e:
            self.log.error('Could not get watch id: {}'.format(str(e)))
            self.watch_id = None

        self.log.info("======> Done Configure DB key watch'er <=========")

    def stop(self):
        super(EssdMonitor, self).stop()
        self.msgListner.stop()
        self.msgListner.join()

        self.running = False

        msg = {'module': 'essd',
               'service': 'monitor',
               'running': False}

        self.essdArg.publisher.send('ufmmonitor', msg)

        self.log.error("====== Watch ID {} ========".format(self.watch_id))

        if not self.db:
            self.log.error("DB is closed")
        else:
            if not self.watch_id:
                self.log.error("Invalid watch ID")
            else:
                pass
                # Find out why cancel doesn't work
                # self.db.cancel_watch(self.watch_id)

        self.log.info('===> Stop Essd <===')

    def is_running(self):
        return self.running

    def _essdMonitor(self, cbArgs):
        """
           This function run in a thread
           Do some monitor work here
        """
        pass
