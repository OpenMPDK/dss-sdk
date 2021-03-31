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

import importlib

from common.ufmlog import ufmlog
from functools import wraps

# Database module to use
db = None


class Ufmdb(object):
    def __new__(cls, **kwargs):
        # Get the database type, load the required module and
        # validate the inputs to the database connection.

        # Change the database to use based on the inputs
        global db
        db_type = kwargs.pop('db_type', 'etcd')
        if db_type == 'etcd':
            db = importlib.import_module('common.ufmdb.ufmdb_etcd3.ufmdb_etcd_client')
        else:
            raise Exception('Invalid database type provided, {} is not valid'.format(db_type))

        # Create an instance of the class only if the input options are valid
        if db.is_valid_init_args(**kwargs):
            return object.__new__(cls)
        else:
            raise Exception('Invalid inputs provided')

    def __init__(self, **kwargs):
        """
        Create connection to database.

        :param kwargs: additional options dictionary
            :param db_type: database type to connect to.
            :type db_type: str, Default: 'etcd'
            :param host: Database host to connect to
            :type host: str, Default: 'localhost' or '127.0.0.1'
            :param port: Database port to connect to
            :type port: int, 'Default':2379

        """
        self.log = ufmlog.log(module="DB", mask=ufmlog.UFM_DB)
        self.log.log_detail_off()

        self.client = db.client(**kwargs)
        self.transactions = self.client.transactions

    def log_error(func):
        @wraps(func)
        def func_wrapper(*args, **kwargs):
            self = args[0]
            try:
                return func(*args, **kwargs)
            except Exception as e:
                self.log.exception(e)
                return None
        return func_wrapper

    def close(self):
        """Close connection to database."""
        if self.client is not None:
            return self.client.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        self.close()

    @log_error
    def put(self, key, value, **kwargs):
        """
        Save a value to the database.

        Example usage:

        .. code-block:: python

            >>> import Ufmdb
            >>> client = ufmdb.client()
            >>> client.put('/thing/key', 'hello world')

        :param key: key to set in database
        :param value: value to set key to
        :type value: bytes
        :param kwargs: additional options dictionary
            :param lease: Lease to associate with this key.
            :type lease:  int (ID of lease)
            :param prev_kv: return the previous key-value pair
            :type prev_kv: bool
        :returns: a response containing a header and other parameters
        """
        self.log.detail("PUT: key=%s value=%s", key, value)
        return self.client.put(key, value, **kwargs)

    @log_error
    def get(self, key):
        """
        Get the value of a key from database.

        example usage:

        .. code-block:: python

            >>> import Ufmdb
            >>> client = ufmdb.client()
            >>> client.get('/thing/key')
            'hello world'

        :param key: key to get from database
        :returns: (value of key, metadata) tuple
        """
        self.log.detail("GET: key="+key)
        return self.client.get(key)

    @log_error
    def get_prefix(self, key_prefix, **kwargs):
        """
        Get a range of keys with a prefix.

        :param key_prefix: first key in range
        :param kwargs: additional options dictionary
            :param sort_order:
            :type sort_order: str, either 'None',
                'ascend', or 'descend'
            :param sort_target:
            :type sort_target: str, either 'key',
                'value','version','create', or 'mod'
        :returns: sequence of (value, metadata) tuples
        """
        self.log.detail("GET_PREFIX: key_prefix="+key_prefix)

        return self.client.get_prefix(key_prefix, **kwargs)

    @log_error
    def get_all(self, **kwargs):
        """
        Get all keys and values currently stored in the database.

        :param kwargs: additional options dictionary
            :param sort_order:
            :type sort_order: str, either 'None',
                'ascend', or 'descend'
            :param sort_target:
            :type sort_target: str, either 'key',
                'value','version','create', or 'mod'
        :returns: sequence of (value, metadata) tuples
        """
        self.log.detail("GET_ALL:")
        return self.client.get_all(**kwargs)

    @log_error
    def delete(self, key, **kwargs):
        """
        Delete a single key in database.

        :param key: key in database to delete
        :param kwargs: additional options dictionary
            :param prev_kv: return the deleted key-value pair
            :type prev_kv: bool
            :param return_response: return the full response
            :type return_response: bool

        :returns: a response containing a header and other parameters
        """
        self.log.detail("DELETE: key="+key)
        return self.client.delete(key, **kwargs)

    @log_error
    def delete_prefix(self, prefix):
        """Delete a range of keys with a prefix in database.

        :param prefix: key prefix in database to delete
        :type prefix: str
        """
        self.log.detail("DELETE_PREFIX: prefix="+prefix)
        return self.client.delete_prefix(prefix)

    @log_error
    def transaction(self, compare_ops, success_ops=None, failure_ops=None):
        """
        Perform a transaction.

        Example usage:

        .. code-block:: python

            >>> import Ufmdb
            >>> client = ufmdb.client()
            >>> client.transaction(
                compare_ops=[
                    client.transactions.value('/doot/testing') == 'doot',
                    client.transactions.version('/doot/testing') > 0,
                ],
                success_ops=[
                    client.transactions.put('/doot/testing', 'success'),
                ],
                failure_ops=[
                    client.transactions.put('/doot/testing', 'failure'),
                ]
            )

        :param compare_ops: A list of comparisons to make
        :param success_ops: A list of operations to perform if all the comparisons
                        are true
        :param failure_ops: A list of operations to perform if any of the
                        comparisons are false
        :return: A tuple of (operation status, responses)
        """
        self.log.detail("TXN: compare_ops=%s success_ops=%s failure_ops=%s", compare_ops, success_ops, failure_ops)
        return self.client.transaction(compare_ops, success_ops, failure_ops)

    @log_error
    def add_watch_callback(self, *args, **kwargs):
        """
        Watch a key or range of keys and call a callback on every response.

        If timeout was declared during the client initialization and
        the watch cannot be created during that time the method raises
        a ``WatchTimedOut`` exception.

        If this function is called by passing only the 'args', the params must
        be passed as
        :param key: key to watch
        :param callback: callback function
        else, the params must be passed as a dictionary of option keywords
        and their values

        :returns: watch_id. It could be used for cancelling watch later
        """
        self.log.detail("ADD_WATCH_CALLBACK:")
        return self.client.add_watch_callback(*args, **kwargs)

    @log_error
    def cancel_watch(self, watch_id):
        """
        Stop watching a key or range of keys.

        :param watch_id: watch_id returned by ``add_watch_callback`` method
        """
        self.log.detail("CANCEL_WATCH: watch_id=%d", watch_id)
        return self.client.cancel_watch(watch_id)

    @log_error
    def lock(self, name, ttl):
        """
        Create a new lock.

        :param name: name of the lock
        :type name: string or bytes
        :param ttl: length of time for the lock to live for in seconds. The
                    lock will be released after this time elapses, unless
                    refreshed
        :type ttl: int
        :returns: new lock
        """
        self.log.detail("LOCK: name="+name+" ttl=%d", ttl)
        return self.client.lock(name, ttl)

    @log_error
    def lease(self, ttl, lease_id=None):
        """
        Create a new lease.

        All keys attached to this lease will be expired and deleted if the
        lease expires. A lease can be sent keep alive messages to refresh the
        ttl.

        :param ttl: Requested time to live
        :type ttl: int
        :param lease_id: Requested ID for the lease
        :type lease_id: int

        :returns: new lease
        """
        self.log.detail("LEASE: ttl=%d lease_id=%d",ttl, lease_id)
        return self.client.lease(ttl, lease_id)

    @log_error
    def get_lease_info(self, lease_id):
        """
        Get information about the lease.

        :param lease_id: Id of the lease to get information from.
        :type lease_id: int
        :returns information about the lease including remaining
        TTL in seconds.
        """
        self.log.detail("GET_LEASE_INFO: lease_id=%d", lease_id)
        return self.client.get_lease_info(lease_id)

    @log_error
    def revoke_lease(self, lease_id):
        """
        Revoke a lease.

        :param lease_id: ID of the lease to revoke.
        :type lease_id: int
        """
        self.log.detail("REVOKE_LEASE: lease_id=%d", lease_id)
        return self.client.revoke_lease(lease_id)

    @log_error
    def members(self):
        """
        Returns list of all members associated with the cluster.

        :rtype: sequence of class with members
            Member id, peer urls and client urls
        """
        self.log.detail("MEMBERS:")
        return self.client.members()

    @log_error
    def status(self):
        """Get the status of the responding member.

        :rtype: sequence of class with members
            db_size, leader, raft_index, raft_term, version
        """

        self.log.detail("STATUS:")
        return self.client.status()

    @log_error
    def watch_callback(self, *args, **kwargs):
        self.log.detail("WATCH_CALLBACK:")
        return self.client.watch_callback(*args, **kwargs)

    @staticmethod
    def __convert_flat_key_list_to_json(etcd_key_dict):
        """Convert the flat key in a list to json format

        :param etcd_key_dict: simple list of keys

        :return: converted json dictionary
        """
        json_dict = dict()
        for k, v in etcd_key_dict.items():
            if type(k) != str:
                k = k.decode()
            if type(v) != str:
                v = v.decode()
            keys = k.split('/')[1:]
            json_kv = json_dict
            for key in keys[:-1]:
                json_kv = json_kv.setdefault(key, {})
            json_kv[keys[-1]] = v
        return json_dict

    @log_error
    def get_with_prefix(self, key_str, raw=False, sort_order=None, sort_target=None):
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
        flat_key_dict = dict()
        try:
            ret = self.client.get_prefix(key_str, sort_order=sort_order, sort_target=sort_target)
            for value, meta in ret:
                flat_key_dict[meta.key] = value

        except Exception:
            self.logger.exception('Got exception getting the key with prefix %s', key_str)

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

    @log_error
    def put_multiple(self, kv_dict, strings_with_lease=None, lease=None):
        """Save multiple Key Values in the ETCD DB.

        Some of the keys can be saved with lease so that they get removed
        automatically upon the lease expiration.
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
        except Exception:
            pass

        return status


def client(**kwargs):
    """Return an instance of Ufmdb_Client."""
    return Ufmdb(**kwargs)
