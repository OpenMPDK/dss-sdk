
import json
import os
import sys
import threading
import time
import socket

from subprocess import PIPE, Popen, STDOUT
import zlib

import requests
from netifaces import interfaces, ifaddresses, AF_INET

from common.clusterlib import lib, lib_constants

from common.events.events import save_events_to_etcd_db, ETCD_EVENT_KEY_PREFIX
from common.events.events import ETCD_EVENT_TO_PROCESS_KEY_PREFIX
from common.events.event_notification import EventNotification

from common.system.monitor import Monitor

from systems.nkv.node_info       import Node_Info
from systems.nkv.node_disk_space import Node_Disk_Space
from systems.nkv.node_health     import Node_Health
from systems.nkv.node_event_processor import Event_Processor

from systems.nkv.node_util import *


POLL_DISK_SPACE_INTERVAL = 120
UPDATE_NODE_INFO_INTERVAL = 60
ZMQ_BROKER_PORT = 6000

# This global get populated by node_health thread
g_servers_out = None


def process_event(db, logger, hostname, event_q, servers_out, event):
    """
      Check for the cluster status key and stop/start the
      keepalived service depending on its status.
    """
    if not event_q:
        logger.error('Processed event is called with invalid event_q object')
        return

    cluster_status_key = '/cluster/' + hostname + '/status'
    if event.key == cluster_status_key:
        if not event.value:
            start_stop_service(logger, 'keepalived', 'stop')
        else:
            old_value = event._event.prev_kv.value
            if old_value != event.value:
                if event.value == 'up':
                    start_stop_service(logger, 'keepalived', 'start')
                else:
                    start_stop_service(logger, 'keepalived', 'stop')

    # Start reading the events with old value and new value and start processing
    if event.key.startswith(ETCD_EVENT_TO_PROCESS_KEY_PREFIX.encode('utf-8')):
        if event.value:
            event_q.add_event_to_worker_queue(event.key, event.value)
        return

    if event.key.startswith(ETCD_EVENT_KEY_PREFIX.encode('utf-8')):
        return

    if event.value:
        old_value = event._event.prev_kv.value
        if old_value == event.value:
            return

    clustername = get_clustername(db)
    if clustername == None:
        logger.error('Could not find cluster name in DB')
        return

    t_event = dict()
    status = format_event(event=t_event,
                          db=db,
                          clustername=clustername,
                          servers_out=servers_out,
                          key=event.key,
                          val=event.value)
    if status == False:
        logger.error('Unhandled event')
        return

    event_list = []
    if t_event:
        t_event['timestamp'] = str(int(time.time()))
        event_list.append(t_event)

    if event_list:
        logger.debug('Saving events to DB %s', str(event_list))
        try:
            save_events_to_etcd_db(db, event_list)
        except Exception as e:
            logger.error('Exception in saving events: %s', str(e))


class NkvMonitor(Monitor):
    def __init__(self, hostname=None, logger=None, db=None):
        Monitor.__init__(self)
        self.hostname = hostname
        self.logger = logger
        self.db = db
        self.thread = None
        self.ev_worker_thread = None
        self.read_system_space = None
        self.update_node_info = None
        self.watch_id = None
        self.logger.info("NkvMonitor hostname = {}".format(self.hostname))

        self.thread_event = threading.Event()
        self.thread_event.clear()

        self.running = False
        self.external_msg_broker = False
        self.en = None

        self.g_event_notifier_fn = None


    def __del__(self):
        self.thread_event.clear()


    def read_node_capacity_in_kb(self):
        df = os.statvfs('/')
        if df.f_blocks > 0:
            return df.f_blocks * 4

        return 0


    def get_ip_of_nic(self):
        '''
         Return the first valid nic ip
         else return (127.0.0.1)
        '''
        localhost = None
        for ifaceName in interfaces():
            addresses = [i['addr'] for i in ifaddresses(ifaceName).setdefault(AF_INET, [{'addr':'No IP addr'}])]
            if ifaceName != 'lo':
                # print('{}: {}'.format(ifaceName, ', '.join(addresses)) )
                return addresses[0]

        return "127.0.0.1"


    # Note: This function can only be called after the start function
    def save_node_capacity(self, capacity_in_kb):
        try:
            self.db.save_key_value('/cluster/' + self.hostname + '/total_capacity_in_kb', str(capacity_in_kb))
        except:
            self.logger.exception('Exception in saving total_capacity_in_kb')


    def get_host_ip(self):
        try:
            cluster_out = self.db.get_key_with_prefix('/cluster/')
            if cluster_out:
                return cluster_out['cluster'][self.hostname]['ip_address']

        except:
            pass

        return None

    def key_watcher_cb(self, event):
        if not isinstance(event.events, list):
            return

        if not self.event_processer:
            self.logger.error("======> Error <=========")

        global g_servers_out
        for e in event.events:
            process_event(db=self.db,
                          logger=self.logger,
                          hostname=self.hostname,
                          event_q=self.event_processer,
                          servers_out=g_servers_out,
                          event=e)

    def start(self):
        self.logger.info("======> NkvMonitor <=========")

        if self.running:
            self.logger.debug("==> NkvMonitor is already running <==")
            return

        vip_address = None
        if self.external_msg_broker:
            vip_address = self.db.get_key_value('/cluster/ip_address')
            vip_address = vip_address.decode('utf-8')
        else:
            vip_address = self.get_ip_of_nic()

        if not vip_address:
            self.logger.error('Could not find VIP address of cluster. Exiting')
            sys.exit(-1)

        self.logger.debug("vip address: [%s]", vip_address)
        if not self.g_event_notifier_fn:
            self.logger.debug('IP for the broker {}'.format(vip_address))
            try:
                self.en = EventNotification(vip_address, ZMQ_BROKER_PORT, logger=self.logger)
                self.g_event_notifier_fn = self.en.process_events
            except:
                self.logger.error('EventNotification instance not created')

        node_ip = self.get_host_ip()
        if not node_ip:
            self.logger.error("Error in getting the node IP address. Exiting")
            sys.exit(-1)

        self.save_node_capacity(self.read_node_capacity_in_kb())

        self.read_system_space = Node_Disk_Space(stopper_event=self.thread_event,
                                                 db=self.db,
                                                 hostname=self.hostname,
                                                 check_interval=POLL_DISK_SPACE_INTERVAL,
                                                 logger=self.logger)
        self.read_system_space.start()

        self.node_info = Node_Info(stopper_event=self.thread_event,
                                   db=self.db,
                                   hostname=self.hostname,
                                   check_interval=UPDATE_NODE_INFO_INTERVAL,
                                   logger=self.logger)
        self.node_info.start()

        self.node_health = Node_Health(stopper_event=self.thread_event,
                                       db=self.db,
                                       hostname=self.hostname,
                                       check_interval=UPDATE_NODE_INFO_INTERVAL,
                                       logger=self.logger)
        self.node_health.start()

        self.event_processer = Event_Processor(stopper_event=self.thread_event,
                                               event_notifier_fn=self.g_event_notifier_fn,
                                               db=self.db,
                                               hostname=self.hostname,
                                               check_interval=UPDATE_NODE_INFO_INTERVAL,
                                               logger=self.logger)
        self.event_processer.start()


        # The watch callback must be set after the event processor is initialize
        self.logger.info("======> Configure DB key watch'er <=========")
        try:
            # New way
            # self.watch_id = self.db.client.watch_callback('/', self.key_watcher_cb, previous_kv=True)

            # Old code was following:
            self.watch_id = self.db.watch_callback('/', self.key_watcher_cb, previous_kv=True)
        except Exception as e:
            self.logger.error('Exception could not get watch id: {}'.format(str(e)))
            self.watch_id = None

        self.running = True
        self.logger.info("======> NkvMonitor has started <=========")


    def stop(self):
        if not self.db:
            self.logger.error("DB should not been closed")
        else:
            # Cancel watcher so no more events get added to the queue
            # when a stop is issued. See [FAB-332]
            if not self.watch_id:
                self.logger.error("Files to cancel DB watch'er")
            else:
                self.db.cancel_watch(self.watch_id)

        self.thread_event.set()

        if self.read_system_space and self.read_system_space.is_alive():
            self.read_system_space.join()

        if self.node_info and self.node_info.is_alive():
            self.node_info.join()

        if self.node_health and self.node_health.is_alive():
            self.node_health.join()
            self.node_health = None

        if self.event_processer and self.event_processer.is_alive():
            self.event_processer.join()
            self.event_processer = None

        self.running = False
        self.logger.info("======> NkvMonitor Stopped <=========")


    def is_running(self):
        return self.running


