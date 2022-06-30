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


import ast
import threading

from common.events import event_constants
import common.events.events_def_en as events_def


# Used to be "process_events_from_worker_queue"
class Event_Processor(threading.Thread):
    def __init__(self, stopper_event=None, event_notifier_fn=None,
                 db=None, hostname=None, check_interval=60, log=None):
        super(Event_Processor, self).__init__()

        self.stopper_event = stopper_event
        self.db = db
        self.event_notifier_fn = event_notifier_fn
        self.hostname = hostname
        self.check_interval = check_interval
        self.log = log

        self.queue_tevent = threading.Event()
        self.queue_tevent.clear()

        self.event_queue = {}

        self.log.debug('Initialize worker queue')
        try:
            # pull all the events listed under /cluster/events_to_process into the dictionary
            res = db.get_with_prefix(event_constants.EVENT_PROCESS_KEY_PREFIX, raw=True)
        except Exception as ex:
            self.log.exception("Exception in getting events to process {}".format(ex))
            return

        if not res:
            self.log.info("==> No events saved in DB to process")
        else:
            for k, v in res.items():
                self.event_queue[k] = v
            self.queue_tevent.set()

    def __del__(self):
        self.log.debug('destroy worker queue')

        self.event_queue = {}
        self.queue_tevent.set()

    def add_event_to_worker_queue(self, key, value):
        self.event_queue[key] = value
        self.queue_tevent.set()

    def remove_event_from_worker_queue(self, key):
        try:
            self.event_queue.pop(key)
        except Exception:
            pass

    def run_global_handlers(self, evt1, event_info):
        try:
            if events_def.global_event_handlers:
                event_fn = events_def.global_event_handlers

                self.log.debug("Global event handler {} on event {}".format(event_fn, evt1))
                ret = event_fn([event_info])
                if ret:
                    if ret != [event_info]:
                        self.log.error("Error in processing event {} with ret None".format(evt1))
                    else:
                        self.log.error("Error in processing event {}".format(evt1))

            # Run local handlers if any
            local_handlers = events_def.events[event_info['name']]['handler']
            if local_handlers:
                for lh in local_handlers:
                    self.log.debug("Local event handler {} on event {}".format(lh, evt1))

                    ret = lh(event_info)
                    if not ret:
                        self.log.error("Failed to process event {} by handler {}".format(event_info, lh))

        except Exception as ex:
            self.log.error("Exception: {} {}".format(__file__, ex))

    def validate_event(self, event_info, event_key, event_value):
        try:
            if self.event_notifier_fn:
                ret = self.event_notifier_fn([event_info])
                ret_status = False
                if ret:
                    if ret == [event_info]:
                        ret_status = True
                    else:
                        self.log.error("Failed to process: {} with {}".format(event_value.decode(), ret))
                else:
                    self.log.error("Failed to process event: {}".format(event_value.decode()))

                if not ret_status:
                    self.add_event_to_worker_queue(event_key, event_value)
                    return True

        except Exception as ex:
            self.log.error("Exception: {} {}".format(__file__, ex))
            return True

        return False

    def run(self):
        self.log.info("======> starting process events from worker queue <=========")

        while not self.stopper_event.is_set():
            if self.event_queue:
                event_tuple = self.event_queue.popitem()
                if event_tuple[1] is None:
                    continue

                evt0 = event_tuple[0].decode("utf-8")
                evt1 = event_tuple[1].decode("utf-8")

                try:
                    event_info = ast.literal_eval(evt1)
                except Exception as ex:
                    self.log.error("Failed to convert event to dict: key={}".format(evt1))
                    self.log.error("Exception: {} {}".format(__file__, ex))
                    continue

                if self.validate_event(event_info=event_info, event_key=event_tuple[0], event_value=event_tuple[1]):
                    continue

                self.run_global_handlers(evt1=evt1, event_info=event_info)

                try:
                    key = evt0.replace(event_constants.EVENT_PROCESS_KEY_PREFIX, event_constants.EVENT_KEY_PREFIX)

                    self.db.put(key, event_tuple[1])
                    self.db.delete(evt0)
                except Exception as ex:
                    self.log.error("Failed to mark event being processed: {}".format(evt0))
                    self.log.error("Exception: {} {}".format(__file__, ex))
            else:
                self.queue_tevent.clear()

            self.queue_tevent.wait(self.check_interval)
