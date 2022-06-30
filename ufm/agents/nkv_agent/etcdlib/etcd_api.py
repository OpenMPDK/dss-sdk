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


"""API interface for ETCD DB
This library handles the ETCD db operations like GET, SAVE, DELETE with and without prefix.
"""
import json
import os

# from enum import Enum
import etcd3 as etcd
from etcd3.utils import increment_last_byte, to_bytes
from flatten_dict import flatten

from logger import cm_logger

ETCDCTL_COMMAND = 'ETCDCTL_API=3 etcdctl'


class EtcdAPI(object):
    """ The class function for the ETCD APIs.
    It provides the ETCD DB interface functions as well as cluster operation functions.
    """
    def __init__(self, ip_address, port=2379, cache_data=False, logger=None):
        """ Constructor function
        :param ip_address: IP Address of the machine hosting the ETCD DB
        :param port: Port number for connecting to ETCD DB. Default 2379
        :param cache_data: Default value is False.
        :param logger: Logger been initialized by other module
        if set True, then the ETCD DB Key Values will be cached using ETCD DB watchers
        :exception Exception: raises exception LoggerHandleFailed or EtcdConnectionFail
        in case of failure
        """
        self._ip_address = ip_address
        self._port = port
        self.client = None
        self.cache_data = cache_data
        self.cached_etcd_data = None
        self.watch_id = None
        if logger:
            self.logger = logger
        else:
            try:
                self.logger = cm_logger.CMLogger('etcd_api').create_logger()
            except Exception as e:
                print('Failed to create logger handle', e.message)
                raise Exception('LoggerHandleFailed')
        self.initialize()

    def initialize(self):
        """Checks the client connection and creates if stale or not created
        :exception Raises an exception if the client connection fails to establish"""
        if not self.client:
            try:
                self.client = self.get_etcd_client_connection()
            except Exception as excp:
                self.logger.error('Exception in the etcd initialization', exc_info=True)
                raise excp

            if self.cache_data:
                key_str = '/'
                self.cached_etcd_data = self.get_key_with_prefix(key_str, raw=True)
                self.watch_id = self.watch_callback(key_str, self.__callback_fn)

    def deinitialize(self):
        """Marks the client connection as disabled.
        Clears all the cached data"""
        if self.cache_data:
            self.client.cancel(self.watch_id)
            self.watch_id = None
            self.cached_etcd_data = None
        self.client = None

    @staticmethod
    def __convert_flat_key_list_to_json(etcd_key_dict):
        """Convert the flat key in a list to json format
        :param etcd_key_dict: simple list of keys
        :return: converted json dictionary
        """
        json_dict = dict()
        for k, v in etcd_key_dict.iteritems():
            keys = k.split('/')[1:]
            json_kv = json_dict
            for key in keys[:-1]:
                json_kv = json_kv.setdefault(key, {})
            json_kv[keys[-1]] = v
        return json_dict

    @staticmethod
    def __convert_json_key_to_flat_key_dict(json_dict):
        """Helper function to convert the JSON format to flat list
        :param json_dict: JSON dictionary of the key, values
        :return: Returns a simple and flat list of key, values
        """
        # flatten function converts to path format using json, but the first "/" will be missing
        flat_dict = flatten(json_dict, reducer='path')
        etcd_key_dict = dict()
        for k, v in flat_dict.iteritems():
            # Add "/" to the key to make it a complete path
            etcd_key_dict["/" + k] = v
        return etcd_key_dict

    def get_etcd_client_connection(self):
        """Helper function to create the etcd client connection
        :return client object
        :exception raises exception in case of failure
        """
        try:
            client = etcd.client(self._ip_address, self._port)
            # The etcd client connection succeeds irrespective of the server running or not
            # Get the value for a dummy non-existent key to see if the service is up
            client.get('/dummy-non-existent-key')
        except Exception as e:
            self.logger.error('Failed to create connection to etcd server', exc_info=True)
            raise Exception('EtcdConnectionFail')

        return client

    def get_key_value(self, key_str):
        """Helper function to get the value for the given key
        :param key_str: Key
        :return: Returns value as a string
        """
        value = None
        if self.cache_data and self.cached_etcd_data:
            if key_str in self.cached_etcd_data:
                value = self.cached_etcd_data[key_str]
        else:
            try:
                value, _ = self.client.get(key_str)
            except Exception as e:
                self.logger.error('Got exception while reading the key %s', key_str, exc_info=True)
        return value

    def get_key_with_prefix(self, key_str, raw=False, sort_order=None,
                            sort_target=None):
        """ Get all the keys with the prefix
        If raw is True, it returns all the DB entries as KV as-is
        If raw is false, then it returns the json converted output
        :param key_str: Key
        :param raw: Default value is False.
                    If true, returns the all the key/value as a dictionary
                    If false, returns JSON format of the key values
        :param sort_order: Option for sorting the order of keys
                           one of 'ascend', 'descend' or None
        :param sort_target: Option for the target
                            one of 'key', 'version', 'create', 'mod', 'value'
                            Default is 'key' if None specified
        :return: Returns JSON formatted or the dictionary with the key/value based on 'raw' field
        """
        if self.cache_data and self.cached_etcd_data:
            flat_key_dict = dict()
            for key, value in self.cached_etcd_data.iteritems():
                if key.startswith(key_str):
                    flat_key_dict[key] = value
        else:
            flat_key_dict = dict()
            try:
                ret = self.client.get_prefix(key_str, sort_order=sort_order,
                                             sort_target=sort_target)
                for value, meta in ret:
                    flat_key_dict[meta.key] = value
            except:
                self.logger.exception('Got exception getting the key with '
                                      'prefix %s', key_str)
        if raw:
            output = flat_key_dict
        else:
            if not flat_key_dict:
                output = dict()
                d = output
                for elem in key_str.split('/')[1:]:
                    d = d.setdefault(elem, {})
            else:
                output = self.__convert_flat_key_list_to_json(flat_key_dict)
        return output

    def save_key_value(self, key_str, value, lease=None):
        """Save Key Value in the DB with or without lease. If lease is set for a time period,
        the key will disappear after that interval. The lease can be set for the key by providing
        the existing lease object created from create_lease or refresh_lease functions.
        :param key_str: Key as a string
        :param value: Value as a string.
                      if given as integer, converted to string
        :param lease: Lease object created with the ETCD DB. Should have minimum remaining ttl of 5 seconds
        :return: Returns True if the key is saved successfully.
                 Else raise exception if invalid lease.
        :exception: Raise Exception('InvalidLease') if invalid lease given
        """
        if lease:
            if lease.remaining_ttl < 5:
                raise Exception('InvalidLease')
        if type(value) == int:
            value = str(value)
        try:
            ret = self.client.put(key_str, value, lease=lease)
        except Exception as e:
            self.logger.error('Got exception in saving key-value %s:%s', key_str, value, exc_info=True)
            ret = False
        return ret

    def save_multiple_key_values(self, kv_dict, strings_with_lease=None, lease=None):
        """Save multiple Key Values in the ETCD DB. Some of the keys can be saved with
        lease so that they get removed automatically upon the lease expiration.
        :param kv_dict: A dictionary with Keys and Values
        :param strings_with_lease: A list of strings (subset of keys in kv_dict) to be set with lease
        :param lease: lease structure created by ETCD DB
        :return: Returns True if successfully saved.
                Else false.
        """
        compare = []
        success = []
        failure = []
        status = False
        try:
            for k, v in kv_dict.iteritems():
                if strings_with_lease and k in strings_with_lease:
                    success.append(self.client.transactions.put(k, v, lease))
                else:
                    success.append(self.client.transactions.put(k, v))
            status, responses = self.client.transaction(compare, success, failure)
            if not status:
                self.logger.error('Error in saving multiple keys. kv_dict %s Error is %s' %
                                  (json.dumps(kv_dict), responses))
        except Exception as e:
            self.logger.error('Exception in saving multiple key value pairs', exc_info=True)
        return status

    def create_lease(self, lease_ttl, lease_id=None):
        """
        Create the lease for the given lease_ttl and lease_id.
        If no lease found, then a lease is created with lease_ttl
        :param lease_ttl: The lease period to be set on the key
        :param lease_id: The lease ID of the previous lease object created with a TTL or your own id
        :return: Returns a lease object
        :exception: Raises an exception if the lease_id is already in use while creating a new lease
                    etcd3.exceptions.PreconditionFailedError
        """
        try:
            lease = self.client.lease(lease_ttl, lease_id)
        except Exception as e:
            self.logger.error('Error in creating the lease', exc_info=True)
            raise e
        return lease

    def refresh_lease(self, lease_ttl=None, lease_id=None, lease=None):
        """
        Refresh the lease for the given lease or lease_id.
        Create a new lease object if no lease or lease_id is given
        If the lease is expired for the given lease or lease_id, then a fresh lease
        is created with the given lease_ttl
        :param lease_ttl: The lease period to be set on the key
        :param lease_id: The lease ID of the previous lease object created with a TTL
        :param lease: Lease object created with the ETCD DB
        :return: Returns a tuple of (lease object,lease_id) if it is created or a valid lease object given.
                 Otherwise it returns (None, lease_id)
                 Returns (None, None) in case of invalid input or failure
        :exception: Raises an exception if the lease_id is already in use while creating a new lease
                    etcd3.exceptions.PreconditionFailedError
                    etcd3.exceptions.ConnectionFailedError
        """
        ret = None
        if not any([lease, lease_id, lease_ttl]):
            return None, None

        key_lease_id = None
        try:
            if lease and lease.remaining_ttl > 0:
                lease.refresh()
                key_lease_id = lease.id
                ret = lease
            if not key_lease_id:
                if lease_id:
                    lease = self.client.get_lease_info(lease_id)
                    if lease.TTL >= 0:
                        leases = list(self.client.refresh_lease(lease_id))
                        key_lease_id = leases[0].ID

                if lease_ttl and not key_lease_id:
                    try:
                        lease = self.client.lease(lease_ttl, lease_id)
                        key_lease_id = lease.id
                        ret = lease
                    except Exception as e:
                        self.logger.error('Error in creating the lease', exc_info=True)
                        raise e
        except Exception as e:
            self.logger.error('Caught exception in refreshing the lease', exc_info=True)
            raise e

        if key_lease_id:
            return ret, key_lease_id
        else:
            return None, None

    def cancel_lease(self, lease=None, lease_id=None):
        """Cancel the lease created by the lease functions
        :param lease: lease object returned by create_lease function
        :param lease_id: Lease ID of the lease object created
        :return Return None on success
        :exception Raises exception in case of invalid parameters or failures
        """
        if not any([lease, lease_id]):
            raise Exception('Invalid parameters')

        try:
            if lease:
                lease.revoke()
            else:
                self.client.revoke_lease(lease_id)
        except Exception as e:
            self.logger.error('Cancel lease failed with error', exc_info=True)
            raise Exception('Invalid lease')

    def delete_key_value(self, key_str):
        """ Delete a key
        :param key_str: Key string to be deleted
        :return Returns True if deleted or False
        """
        try:
            ret = self.client.delete(key_str)
        except Exception as e:
            self.logger.error('Exception in deleting key %s', key_str, exc_info=True)
            ret = False
        return ret

    def delete_key_with_prefix(self, key_str):
        """Delete all the keys starting with the given prefix
        :param key_str: Prefix of all the key strings to be deleted
        :return: True if deleted. Else false
        """
        try:
            ret = self.client.delete_prefix(key_str)
        except Exception as e:
            self.logger.error('Exception in deleting key %s', key_str, exc_info=True)
            ret = False
        return ret

    def __callback_fn(self, event):
        """Callback function to cache the etcd changes"""
        # self.logger.info("Callback events received %s %s" % (event.key, event.value or None))
        if self.cache_data:
            if not event.value:
                if event.key in self.cached_etcd_data:
                    self.cached_etcd_data.pop(event.key)
            else:
                self.cached_etcd_data[event.key] = event.value

    def watch_callback(self, key_str, cb_fn, previous_kv=False):
        """A watcher function on the ETCD keys. The value added/changed/deleted will be
        sent to the callback function with the structure
        event = {'key': "KEY", 'value':"VALUE"}
        If the event.value is empty, then the key is deleted on the DB
        Else, the key is added/modified.
        :param key_str: Prefix Key string to watch
        :param cb_fn: Callback function to received the changes in the DB
        :param previous_kv: To get the previous KV for the key getting changed or deleted
        :return: Returns the watcher id which can be used to cancel the watcher if needed
        """
        try:
            watch_id = self.client.add_watch_callback(key_str, cb_fn,
                                                      range_end=increment_last_byte(to_bytes(key_str)), prev_kv=previous_kv)
        except Exception as e:
            self.logger.error('Exception in creating watcher callback - %s', key_str, exc_info=True)
            watch_id = None
        return watch_id

    def get_db_last_update_time(self, cluster_name):
        """Returns the last update time of the DB file.
        The ETCD db file is stoed in /var/lib/etcd/<cluster_name>.etcd/member/snap/db.
        This function returns the time stamp of the file using stat command
        :param cluster_name: Name of the cluster created
        :return: timestamp of the file or None
        """
        filename = '/var/lib/etcd/' + cluster_name + '.etcd/member/snap/db'
        ctime = None
        try:
            fs_stat = os.stat(filename)
            ctime = fs_stat.st_ctime
        except Exception as excp:
            self.logger.error('Error in getting the stat information on the file %s with exception %s' %
                              (filename, excp.message))
        return ctime

    def get_status(self):
        """Returns the status object returned from the ETCD DB client.
        The object contains the following elements
        {'db_size': <val>, 'leader': <member object>, 'raft_index': <val>, 'raft_term': <val>, 'version': <val>}
        :exception Raises an exception in case of an exception from the DB client connection
        """
        try:
            status = self.client.status()
        except Exception as e:
            self.logger.error('Caught exception while getting the status', exc_info=True)
            raise e
        return status

    def get_members(self):
        """Returns an array of member objects returned by the ETCD DB client members call
        Each member object contain the following elements
        {'active_alarms', 'client_urls', 'id', 'name', 'peer_urls', 'remove', 'update'}
        > str(<member object>)
        > "Member 16002936010456593402: peer urls: [u'http://10.1.50.148:2380'], client urls: [u'http://10.1.50.148:2379']"
        :return Returns None in case of exception
        """
        try:
            members = self.client.members
        except Exception as e:
            self.logger.error('Caught exception while getting the member list', exc_info=True)
            members = None
        return members


if __name__ == "__main__":
    etcd = EtcdAPI('127.0.0.1')
    etcd_data = etcd.get_key_with_prefix('/')
    print(etcd_data)
    etcd_data = etcd.get_key_value('/cluster')
    print(etcd_data)
