
import os
import sys
import ast
import threading

import common.events.events_def_en as events_def
from common.events.events import save_events_to_etcd_db, ETCD_EVENT_KEY_PREFIX
from common.events.events import ETCD_EVENT_TO_PROCESS_KEY_PREFIX
from common.events.event_notification import EventNotification


# Used to be "process_events_from_worker_queue"
class Event_Processor(threading.Thread):
    def __init__(self, stopper_event=None, event_notifier_fn=None, db=None, hostname=None, check_interval=60, logger=None):
        super(Event_Processor, self).__init__()

        self.stopper_event = stopper_event
        self.db = db
        self.event_notifier_fn = event_notifier_fn
        self.hostname = hostname
        self.check_interval = check_interval
        self.logger = logger

        self.queue_tevent = threading.Event()
        self.queue_tevent.clear()

        self.event_queue = {}

        self.logger.debug('Initialize worker queue')
        try:
            # pull all the events listed under /cluster/events_to_process into the dictionary
            res = db.get_key_with_prefix(ETCD_EVENT_TO_PROCESS_KEY_PREFIX, raw=True)
            if not res:
                self.logger.debug('Dictonary is empty')
                return
        except:
            self.logger.exception('Exception in getting events to process')
            return

        for k, v in res.items():
            self.event_queue[k] = v
        self.queue_tevent.set()


    def __del__(self):
        self.logger.debug('destroy worker queue')

        self.event_queue = {}
        self.queue_tevent.set()


    def add_event_to_worker_queue(self, key, value):
        self.event_queue[key] = value
        self.queue_tevent.set()


    def remove_event_from_worker_queue(key):
        try:
            self.event_queue.pop(key)
        except:
            pass


    def run(self):
        self.logger.info("======> starting process events from worker queue <=========")
        while not self.stopper_event.is_set():
            if self.event_queue:
                event_tuple = self.event_queue.popitem()
                try:
                    evt0 = event_tuple[0].decode("utf-8")
                    evt1 = event_tuple[1].decode("utf-8")

                    # process the event  Validate event
                    if 'null' in evt1 or 'None' in evt1:
                        self.logger.error('Malformed event, will skip - {}'.format(event_tuple[1]))
                        continue

                    # convert the event in string format to dict
                    event_info = ast.literal_eval(evt1)
                    self.logger.debug('Processing the event %s', event_tuple[1])
                    if self.event_notifier_fn:
                        ret = self.event_notifier_fn([event_info])
                        ret_status = False
                        if ret:
                            if ret == [event_info]:
                                ret_status = True
                            elif not ret:
                                self.logger.error('Error in processing event %s with ret None', event_tuple[1])
                            else:
                                self.logger.error('Error in processing event %s with ret %s', event_tuple[1], str(ret))
                        if not ret_status:
                            self.add_event_to_worker_queue(event_tuple[0], event_tuple[1])
                            continue

                    # Run global handlers
                    if events_def.global_event_handlers:
                        event_fn = events_def.global_event_handlers
                        self.logger.debug('Global event handler %s on event %s', event_fn, event_tuple[1])
                        ret = event_fn([event_info])
                        if ret:
                            if not ret or ret != [event_info]:
                                self.logger.error('Error in processing event %s with ret None', event_tuple[1])
                            else:
                                self.logger.error('Error in processing event %s with ret %s', event_tuple[1], str(ret))

                    # Run local handlers if any
                    local_handlers = events_def.events[event_info['name']]['handler']
                    if local_handlers:
                        for lh in local_handlers:
                            self.logger.debug('Local event handler %s on event %s', lh, event_tuple[1])
                            ret = lh(event_info)
                            if not ret:
                                self.logger.error('Error in processing event %s by handler %s', str(event_info), lh)

                    self.logger.debug('Sending the event %s to MQ library', event_tuple[1])

                    key = evt0.replace(ETCD_EVENT_TO_PROCESS_KEY_PREFIX, ETCD_EVENT_KEY_PREFIX)
                    self.logger.debug('Updating the key %s in DB', key)
                    self.db.save_key_value(key, event_tuple[1])

                    self.logger.debug('Deleting {} in DB'.format(event_tuple[0].decode("utf-8")))
                    self.db.delete_key_value(event_tuple[0].decode("utf-8"))
                except Exception as e:
                    self.logger.error('Exception %s in processing the event %s', e, event_tuple[1])
                    # raise
            else:
                self.queue_tevent.clear()

            self.queue_tevent.wait(self.check_interval)

