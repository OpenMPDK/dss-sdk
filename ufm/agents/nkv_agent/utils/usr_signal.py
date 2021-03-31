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

import signal

from log_setup import agent_logger


class UDEVMonitorSignalHandler:
    def __init__(self, poll_event, mode_event,
                 start_threads_fn=None, stop_threads_fn=None):
        self.poll_event = poll_event
        self.mode_event = mode_event
        self.start_monitor_threads_fn = start_threads_fn
        self.stop_monitor_threads_fn = stop_threads_fn
        self.exit = 0
        self.logger = agent_logger

    def signal_handler_sigint(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGINT ====')
        else:
            print('==== Received SIGINT ====')
        self.exit = 1
        self.mode_event.set()

    def signal_handler_sigusr1(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGUSR1 ====')
        else:
            print('==== Received SIGUSR1 ====')
        if self.start_monitor_threads_fn:
            self.logger.info('Calling start monitor threads function')
            self.start_monitor_threads_fn()

    def signal_handler_sigusr2(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGUSR2 ====')
        else:
            print('==== Received SIGUSR2 ====')
        if self.stop_monitor_threads_fn:
            self.logger.info('Calling stop monitor threads function')
            self.stop_monitor_threads_fn()

    def signal_handler_sighup(self, signal, frame):
        if self.logger:
            self.logger.info('==== Received SIGHUP ====')
        else:
            print('==== Received SIGHUP ====')
        self.poll_event.set()

    def catch_SIGUSR1(self):
        signal.signal(signal.SIGUSR1, self.signal_handler_sigusr1)

    def catch_SIGUSR2(self):
        signal.signal(signal.SIGUSR2, self.signal_handler_sigusr2)

    def catch_SIGINT(self):
        signal.signal(signal.SIGINT, self.signal_handler_sigint)

    def catch_SIGHUP(self):
        signal.signal(signal.SIGHUP, self.signal_handler_sighup)

    def catch_signal(self):
        self.catch_SIGHUP()
        self.catch_SIGINT()
        self.catch_SIGUSR1()
        self.catch_SIGUSR2()
