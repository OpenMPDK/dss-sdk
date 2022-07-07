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


# import json
import re
# import sys
# from uuid import uuid1

import etcd3 as etcd3
# import pkg_resources  # Resolves pyinstaller issues with etcd3
from six.moves import queue

from clusterlib.lib import create_db_connection
from events.events import save_events_to_etcd_db
from log_setup import get_logger
from utils import flat_dict_generator


class BackendLayerException(Exception):
    pass


class BackendLayer:
    def __init__(self, endpoint='127.0.0.1', port=23790):
        self.logger = get_logger()
        self.ETCD_OBJ_ROOT = '/object_storage/'
        self.ETCD_SRV_BASE = self.ETCD_OBJ_ROOT + 'servers/'
        self.endpoint = endpoint
        self.port = port
        try:
            self.db_handle = create_db_connection(endpoint, port=port,
                                                  log=self.logger)
            # self.client = self.init_client(self.endpoint, self.port)
            self.client = self.db_handle.client
        except Exception as e:
            self.logger.exception('Exception in initializing the DB client')
            raise e
        self.etcd_target_lock = None

    def set_lock(self, server_uuid, ttl=5):
        try:
            if self.etcd_target_lock and self.etcd_target_lock.is_acquired():
                self.logger.error("Lock acquired while trying to overwrite "
                                  "lock var")
                raise BackendLayerException('Lock already acquired')
            # Create lock object for the given server UUID
            self.etcd_target_lock = self.client.lock(str(server_uuid), ttl=ttl)
        except Exception as e:
            self.logger.exception('Exception in creating DB lock for the '
                                  'server UUID %s', server_uuid)
            raise e

    def acquire_lock(self, timeout=None):
        try:
            if self.etcd_target_lock is None:
                self.logger.error("Invalid lock to acquire")
                raise BackendLayerException('Invalid lock handle to acquire '
                                            'lock')
            self.etcd_target_lock.acquire(timeout)
        except Exception as e:
            self.logger.exception('Exception in acquiring DB lock %s',
                                  self.etcd_target_lock.name)
            raise e

    def release_lock(self):
        try:
            if self.etcd_target_lock is None:
                self.logger.error("Invalid lock to release")
                raise BackendLayerException('Lock already released')
            self.etcd_target_lock.release()
        except Exception as e:
            self.logger.exception('Exception in releasing DB lock %s',
                                  self.etcd_target_lock.name)
            raise e

    def get_json_prefix(self, key_prefix):
        """
        Retrieve all keys for a given key_prefix and return it
        in a dictionary object.
        :param key_prefix: Key prefix to query etcdv3 for.
        :return: Dictionary object filled with query results.
        """
        try:
            all_keys = self.client.get_prefix(key_prefix)
        except Exception as e:
            self.logger.exception('Exception in getting the values for the '
                                  'prefix %s', key_prefix)
            raise e
        d_final = {}
        for value, metadata in all_keys:
            keys = re.sub(key_prefix, '', metadata.key).split('/')
            d = d_final
            for key in keys[:-1]:
                if key not in d:
                    d[key] = {}
                d = d[key]
            d[keys[-1]] = value
        return d_final

    def write_dict_to_etcd(self, _dict, key_prefix=''):
        """
        Flatten dictionary object to etcdv3 commands.
        Perform transactional write to etcdv3 database.
        :param _dict: Dictionary object to flatten.
        :param key_prefix: Prefix to prepend to key.
        :return:
        """
        compare = []
        success = []
        failure = []

        for i in flat_dict_generator(_dict):
            key = key_prefix + '/'.join(i[:-1])
            value = str(i[-1])
            success.append(self.client.transactions.put(key, value))
        try:
            ret = self.client.transaction(compare,
                                          success,
                                          failure)
        except Exception as e:
            self.logger.exception('Exception in writing the values to DB')
            raise e
        return ret

    def write_spdk_command(self, server_uuid, section, kv):
        """
        Writes subsystem arguments/attributes to the server's
        configuration section under the provided NQN.
        E.g. '/object_storage/<server_uuid>/kv_attributes/config/subsystems/<nqn>/<subsystem_attrs>'
        :param server_uuid:
        :param section: NQN to write under the server_uuid and config prefix.
        :param kv: Arguments/attributes that describe this subsystem.
        :return:
        """
        try:
            if self.etcd_target_lock.is_acquired():
                server_base_dir = "%s%s" % (self.ETCD_SRV_BASE, server_uuid)
                server_config_dir = "%s%s" % (server_base_dir,
                                              "/kv_attributes/config/subsystems/")
                server_subsystem_dir = "%s%s/" % (server_config_dir, section)

                subsystem_path = server_uuid + "_" + section
                subsystem_lock = self.client.lock(subsystem_path, ttl=5)
                subsystem_lock.acquire()
                if subsystem_lock.is_acquired():
                    # Callback and provide us the events
                    event_queue = queue.Queue()

                    def callback(_event):
                        event_queue.put(_event)

                    watch_key = server_subsystem_dir + "cmd_status"
                    watch_id = self.client.add_watch_callback(watch_key, callback)

                    compare = []
                    success = []
                    failure = []
                    for i in flat_dict_generator(kv):
                        key = server_subsystem_dir + '/'.join(i[:-1])
                        value = str(i[-1])
                        success.append(self.client.transactions.put(key, value))
                    try:
                        self.client.transaction(compare,
                                                success,
                                                failure)
                    except Exception as e:
                        self.logger.exception("Error completing transaction")
                        subsystem_lock.release()
                        raise e
                    subsystem_lock.release()
                else:
                    self.logger.error("Failed to acquire lock")
                    return None, None
                return event_queue, watch_id
            else:
                self.logger.error("write_command requires lock")
                raise BackendLayerException('Write Command requires lock')
        except Exception as e:
            self.logger.exception('Exception in writing SPDK command to DB')
            raise e

    def event_response(self, event_queue, watch_id):
        # First watch for kv write transaction
        # Second watch for processing from daemon
        # Third watch for completion or failure from daemon
        event = None
        last_state = None
        for i in range(3):
            try:
                event = event_queue.get(timeout=180)
                if event.value:
                    last_state = event.value
                else:
                    last_value = 'deleted'
                self.logger.info('Event data %s', str(event))
            except queue.Empty:
                self.client.cancel_watch(watch_id)
                if not last_state:
                    return None, "Database did not receive configuration to " \
                                 "add/remove subsystem"
                elif last_state == 'pending':
                    return None, "Agent not yet processed the subsystem " \
                                 "command. Taking longer than expected"
                elif last_state == 'processing':
                    return None, "Subsystem command taking longer than " \
                                 "expected"
                elif last_state == 'success':
                    return event, "Success"
                elif last_state == 'deleted':
                    return event, 'Subsystem successfully deleted'
            except Exception as e:
                self.logger.exception("Unknown error - " + str(e))
                raise
        try:
            self.client.cancel_watch(watch_id)
        except Exception as e:
            self.logger.exception('Ignoring the exception in cancelling the '
                                  'DB watcher after multiple retries')
        return event, "Success"

    def init_client(self, ip_address="127.0.0.1", port=None, user=None, pw=None):
        """
        Set default IP address, port, username, and password to use
        when connecting to etcdv3.
        :param ip_address: Endpoint for etcdv3 database.
        :param port: Port for etcdv3 database (Default 2379 for etcdv3).
        :param user: Username to access database.
        :param pw: Password to access database.
        :return: Initialized client object to query or write
                 to database.
        """
        if int(port) != 2379 and int(port) != 23790:
            self.logger.error("Check port if connection issue encountered")
        print("Connecting to etcd: %s:%s" % (str(ip_address), str(port)))
        self.logger.info("Connecting to etcd: %s:%s" % (str(ip_address),
                                                        str(port)))
        try:
            client = etcd3.client(host=ip_address, port=int(port),
                                  ca_cert=None, cert_key=None,
                                  cert_cert=None, timeout=None,
                                  user=user, password=pw)
        except Exception as e:
            self.logger.exception('Exception in initializing the DB client')
            raise e
        return client

    def save_target_agent_events(self, event_list):
        """Save multiple events to DB
        A sample event will be in the following format
        event = {'name': 'CM_NODE_UP',
                 # Node of event origination
                 'node': 'NODE-1',
                 # Arguments to replace the message for the 'CM_NODE_UP'
                 'args': "{'node': 'NODE-1', 'cluster': 'CLS'} ",
                 'timestamp': "<Time string in epoch>"}

        :param event_list: List of events
        :return: True if successful or false.
        :exception Raises an exception in case of DB exception
        """
        try:
            ret = save_events_to_etcd_db(self.db_handle, event_list)
        except:
            raise
        return ret
