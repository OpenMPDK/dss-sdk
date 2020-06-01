import os
import os.path
import sys
import threading
import json
import signal
import time
import datetime

from ufm_thread import UfmThread


class EssdMonitorArg():
    """
       Variables for the thread function and the trigger function
    """
    def __init__(self):
        self.first = False
        self.n = 42


def essdMonitor(cbArgs):
    """
       This function run in a thread
       Do some monitor work here
    """
    print("_M_", flush=True, end='')


def essdMonitorCallback(db=None, logger=None, cbArgs=None, event=None):
    """
       This function is call for every update in dB
    """
    print("========> Process event <=======")
    print("_MC_", flush=True, end='')


class EssdMonitor(UfmThread):
    """
       Description:
          This thread is monitor Essd's and add the Essd'd
          metadata to DB
    """
    def __init__(self, ufmArg=None, essdArg=None, monitor=None, monitorArgs=None, monitorCallback=None):
        self.hostname = ufmArg.hostname
        self.logger = ufmArg.log
        self.db = ufmArg.db
        self.monitor = monitor
        self.monitorArgs = monitorArgs
        self.monitorCallback = monitorCallback
        self.running = False
        self.watch_id = None
        super(EssdMonitor, self).__init__()
        self.logger.info('===> Init Monitor <===')


    def __del__(self):
        if self.running:
            self.stop()
        self.logger.info('===> Delete Essd Monitor <===')


    def __watcher_essd_key_cb(self, event):
        print("========> The monitor got an event <=======")
        if not isinstance(event.events, list):
            return

        if self.monitorCallback:
            self.monitorCallback(self.db, self.logger, self.monitorArgs, event)


    def start(self):
        self.logger.info('===> Start Essd Monitor <===')
        self.running = True
        super(EssdMonitor, self).start(threadName='EssdMonitor', cb=self.monitor, cbArgs=self.monitorArgs, repeatIntervalSecs=2.0)


        self.logger.info("======> Configure DB key watch'er <=========")
        try:
            self.watch_id = self.db.watch_callback('/essd', self.__watcher_essd_key_cb, previous_kv=True)

            if not self.watch_id:
                self.logger.error("=====> watch_callback returned None")

        except Exception as e:
            logger.error('Exception could not get watch id: {}'.format(str(e)))
            self.watch_id = None

        self.logger.info("======> Done Configure DB key watch'er <=========")


    def stop(self):
        super(EssdMonitor, self).stop()
        self.running = False

        self.logger.error("====== Watch ID {} ========".format( self.watch_id ))

        if not self.db:
            self.logger.error("DB is closed")
        else:
            if not self.watch_id:
                self.logger.error("Invalid watch ID")
            else:
                pass
                # Find out why cancel doesn't work
                # self.db.cancel_watch(self.watch_id)

        self.logger.info('===> Stop Essd <===')


    def is_running(self):
        return self.running


