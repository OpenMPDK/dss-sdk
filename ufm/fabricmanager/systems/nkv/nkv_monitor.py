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


import sys
import threading
import time
import json
from uuid import uuid1

from netifaces import interfaces, ifaddresses, AF_INET, gateways

from common.events import event_constants
from common.events.event_notification import EventNotification

from systems.nkv.node_health import Node_Health
from systems.nkv.node_event_processor import Event_Processor

from systems.nkv.node_util import format_event
from systems.nkv.node_util import get_clustername
from systems.nkv.node_util import start_stop_service


POLL_DISK_SPACE_INTERVAL = 120
UPDATE_NODE_INFO_INTERVAL = 60
ZMQ_BROKER_PORT = 6000

# This global get populated by node_health thread
g_servers_out = None


def save_events_to_db(db, event_list, log=None):
    if not isinstance(event_list, list):
        return

    for event in event_list:
        if (('name' not in event) or ('timestamp' not in event) or ('node' not in event)):
            log.info("Skip event {}".format(event))
            continue

        ev_timestamp = str(event['timestamp'])
        ev_uuid1 = str(uuid1())
        ev_key = '/'.join([event_constants.EVENT_PROCESS_KEY_PREFIX, ev_timestamp, ev_uuid1])

        try:
            ev_value = json.dumps(event)
        except Exception as ex:
            log.error("Failed to convert to json {}".format(event))
            log.error("Exception: {} {}".format(__file__, ex))
            continue

        try:
            db.put(ev_key, ev_value)
        except Exception as ex:
            log.error("Failed to save data to db: {}".format(event))
            log.error("Exception: {} {}".format(__file__, ex))


def process_event(db, log, hostname, event_q, servers_out, event):
    """
      Check for the cluster status key and stop/start the
      keepalived service depending on its status.
    """
    if not event_q:
        log.error('Processed event is called with invalid event_q object')
        return

    cluster_status_key = "/cluster/{}/status".format(hostname)
    if event.key == cluster_status_key:
        if not event.value:
            start_stop_service(log=log, service_name='keepalived', action='stop')
        else:
            old_value = event._event.prev_kv.value
            if old_value != event.value:
                if event.value == 'up':
                    start_stop_service(log=log, service_name='keepalived', action='start')
                else:
                    start_stop_service(log=log, service_name='keepalived', action='stop')

    # Start reading the events with old value and new value and start processing
    if event.key.startswith(event_constants.EVENT_PROCESS_KEY_PREFIX.encode('utf-8')):
        if event.value:
            event_q.add_event_to_worker_queue(event.key, event.value)
        return

    if event.key.startswith(event_constants.EVENT_KEY_PREFIX.encode('utf-8')):
        return

    if event.value:
        old_value = event._event.prev_kv.value
        if old_value == event.value:
            return

    clustername = get_clustername(db)
    if not clustername:
        log.error('Could not find cluster name in DB')
        return

    t_event = dict()
    status = format_event(event=t_event,
                          db=db,
                          log=log,
                          clustername=clustername,
                          servers_out=servers_out,
                          key=event.key,
                          val=event.value)
    if not status:
        # log.debug("Unhandled event: {}".format(t_event))
        return

    event_list = []
    if t_event:
        t_event['timestamp'] = str(int(time.time()))
        event_list.append(t_event)

    save_events_to_db(db, event_list, log=log)


class NkvMonitor(object):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self.hostname = ufmArg.hostname
        self.log = ufmArg.log
        self.db = ufmArg.db
        self.thread = None
        self.ev_worker_thread = None
        self.update_node_info = None
        self.watch_id = None

        self.thread_event = threading.Event()
        self.thread_event.clear()

        self.running = False
        self.en = None
        self.g_event_notifier_fn = None

        try:
            self.useBrokerIpFromDb = self.ufmArg.ufmConfig['brokerIpFromDb']
        except Exception:
            self.useBrokerIpFromDb = False

        try:
            self.brokerPort = self.ufmArg.ufmConfig['brokerPort']
        except Exception:
            self.brokerPort = ZMQ_BROKER_PORT

        self.log.info("Init {}".format(self.__class__.__name__))

    def __del__(self):
        self.thread_event.clear()

    def get_ip_of_nic(self):
        '''
         Return the first valid nic ip
         else return (127.0.0.1)
        '''

        gways = gateways()
        default_ifc = gways['default'][AF_INET][1]
        default_ipaddr = ifaddresses(default_ifc)[AF_INET][0]['addr']

        return default_ipaddr if default_ipaddr is not None else 'No IP addr'

    def get_host_ip(self):
        try:
            cluster_out = self.db.get_with_prefix('/cluster/')
            if cluster_out:
                return cluster_out['cluster'][self.hostname]['ip_address']
        except Exception as ex:
            self.log.error("Failed to read IP address from db ({})".format(ex))

        return None

    def key_watcher_cb(self, event):
        if not isinstance(event.events, list):
            return

        if not self.event_processer:
            self.log.error("======> Error <=========")

        global g_servers_out
        for e in event.events:
            process_event(db=self.db,
                          log=self.log,
                          hostname=self.hostname,
                          event_q=self.event_processer,
                          servers_out=g_servers_out,
                          event=e)

    def start(self):
        self.log.info("======> NkvMonitor <=========")

        if self.running:
            self.log.debug("==> NkvMonitor is already running <==")
            return

        vip_address = None
        if self.useBrokerIpFromDb:
            vip_address, _ = self.db.get('/cluster/ip_address')
            vip_address = vip_address.decode('utf-8')
        else:
            vip_address = self.get_ip_of_nic()

        if not vip_address or vip_address == 'No IP addr':
            self.log.error('Could not find VIP address of cluster. Exiting')
            sys.exit(-1)

        self.log.debug("vip address: [%s]", vip_address)

        if not self.g_event_notifier_fn:
            self.log.debug('IP for the broker {}'.format(vip_address))
            try:
                self.en = EventNotification(vip_address, self.brokerPort, logger=self.log)
                self.g_event_notifier_fn = self.en.process_events
            except Exception as ex:
                self.log.error('EventNotification instance could not be created')
                self.log.error("Exception: {} {}".format(__file__, ex))

        node_ip = self.get_host_ip()
        if not node_ip:
            self.log.error("Error in getting the node IP address. Exiting")
            sys.exit(-1)

        self.node_health = Node_Health(stopper_event=self.thread_event,
                                       db=self.db,
                                       hostname=self.hostname,
                                       check_interval=UPDATE_NODE_INFO_INTERVAL,
                                       log=self.log)
        self.node_health.start()

        self.event_processer = Event_Processor(stopper_event=self.thread_event,
                                               event_notifier_fn=self.g_event_notifier_fn,
                                               db=self.db,
                                               hostname=self.hostname,
                                               check_interval=UPDATE_NODE_INFO_INTERVAL,
                                               log=self.log)
        self.event_processer.start()

        # The watch callback must be set after the event processor is initialize
        self.log.info("======> Configure DB key watcher <=========")
        try:
            self.watch_id = self.db.watch_callback('/', self.key_watcher_cb, prev_kv=True)

            self.log.info('==> Watch id: {}'.format(self.watch_id))
        except Exception as ex:
            self.log.error('Exception could not get watch id: {}'.format(ex))
            self.watch_id = None

        self.running = True
        self.log.info("======> NkvMonitor has started <=========")

    def stop(self):
        if not self.db:
            self.log.error("DB should not been closed")
        else:
            if not self.watch_id:
                self.log.error("Failed to cancel DB watcher")
            else:
                self.db.cancel_watch(self.watch_id)

        self.thread_event.set()

        if self.node_health and self.node_health.is_alive():
            self.node_health.join()
            self.node_health = None

        if self.event_processer and self.event_processer.is_alive():
            self.event_processer.join()
            self.event_processer = None

        self.running = False
        self.log.info("======> NkvMonitor Stopped <=========")

    def is_running(self):
        return self.running
