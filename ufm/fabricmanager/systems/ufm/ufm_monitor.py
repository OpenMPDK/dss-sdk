"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import threading

from ufm_thread import UfmThread
from systems.ufm import ufm_constants
from systems.ufm_message import Subscriber


class UfmMonitor(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self.event = threading.Event()

        self.log = self.ufmArg.log
        self.db = self.ufmArg.db
        self.log.info("Init {}".format(self.__class__.__name__))

        self.msgListner = Subscriber(event=self.event,
                                     ports=(self.ufmArg.ufmPorts),
                                     topics=('ufmmonitor',),
                                     process=self.processMonitorrMessages)
        self._running = False
        self.watch_id = None
        super(UfmMonitor, self).__init__()

    def __del__(self):
        pass
        # self.stop()

    def _ufmMonitorCallback(self, event=None):
        if not isinstance(event.events, list):
            return

        # There has been a changed in the /ufm section of the db
        pass

    def _ufmMonitor(self, cbArgs):
        """
           This function run in a thread
           Do some monitor work here
        """
        msg = {'module': 'ufm',
               'service': 'monitor',
               'status': True}

        cbArgs.publisher.send('ufmcontroller', msg)

    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))
        self.msgListner.start()

        self._running = True
        super(UfmMonitor, self).start(threadName='UfmMonitor',
                                      cb=self._ufmMonitor,
                                      cbArgs=self.ufmArg,
                                      repeatIntervalSecs=7.0)

        try:
            self.watch_id = self.db.watch_callback(ufm_constants.UFM_PREFIX, self._ufmMonitorCallback)
        except Exception as e:
            self.log.error('UFM: Could not set callback (key={}) (id={})'.format(ufm_constants.UFM_PREFIX, e))
            self.watch_id = None

        self.log.info("====> UFM: Done Configure DB key watch'er <====")

    def stop(self):
        super(UfmMonitor, self).stop()
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False

        if not self.db:
            self.log.error("DB is closed")
        else:
            if not self.watch_id:
                self.log.error("UFM module: invalid watch ID")
            else:
                self.db.cancel_watch(self.watch_id)

        self.log.info("Stop {}".format(self.__class__.__name__))

    def is_running(self):
        return self._running

    def processMonitorrMessages(self, topic, message):
        # print("Topic: {}  Message to process is: {}\n".format(topic, message))

        # this will send a Aka to publisher
        msg_ok = dict()
        msg_ok['status'] = True

        self.ufmArg.publisher.send("Aka", msg_ok)
