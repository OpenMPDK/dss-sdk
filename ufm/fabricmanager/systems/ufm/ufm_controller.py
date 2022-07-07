# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import threading

from ufm_thread import UfmThread
from systems.ufm_message import Subscriber


class UfmController(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=(self.ufmArg.ufmPorts),
                                     topics=('ufmcontroller',),
                                     process=self.processControllerMessages)
        super(UfmController, self).__init__()
        self.ufmArg.log.info("Init {}".format(self.__class__.__name__))

    def __del__(self):
        self.ufmArg.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass

    def start(self):
        self.ufmArg.log.info("Start {}".format(self.__class__.__name__))
        self.msgListner.start()

        self._running = True

        # Controller might not need a thread
        #   Commented out for now
        # super(UfmController, self).start(threadName='UfmController',
        #                                  cb=self.controller,
        #                                  cbArgs=self.ufmArg,
        #                                  repeatIntervalSecs=3.0)

    # def controller(self, ufmArg): #
    #    pass

    def stop(self):
        # super(UfmController, self).stop()
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False
        self.ufmArg.log.info("Stop {}".format(self.__class__.__name__))

    def is_running(self):
        return self._running

    def processControllerMessages(self, topic, message):
        # print("Topic: {}  Message to process is: {}\n".format(topic,
        #                                                      message))

        # this will send a Aka to publisher
        msg = {'module': 'ufm',
               'service': 'controller',
               'commandcompleted': True}

        self.ufmArg.publisher.send('ufmmonitor', msg)
