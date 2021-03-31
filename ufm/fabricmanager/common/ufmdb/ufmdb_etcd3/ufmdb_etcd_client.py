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

import etcd3 as etcd

from common.ufmdb import ufmdb_client


class Ufmdb_etcd3_Client(ufmdb_client.UfmdbClient):
    def __init__(self, **kwargs):
        # ca_cert=None, cert_key=None, cert_cert=None, timeout=None,
        # user=None, password=None, options=None):
        """
        Create database connection.
        """
        # host and the port are treated separately to provide
        # default values.
        # db_type is ufmdb specific and should be removed before
        # invoking the connection
        kwargs.pop('db_type', 'etcd_db')
        host = kwargs.pop('host', 'localhost')
        port = kwargs.pop('port', 2379)
        self.client = etcd.client(host=host, port=port, **kwargs)
        self.transactions = self.client.transactions

    def close(self):
        """Close connection to database."""
        if self.client is not None:
            return self.client.close()

    def put(self, key, value, **kwargs):
        """
        Save a value to etcd.

        :param key: key in etcd to set
        :param value: value to set key to
        :type value: bytes
        :param kwargs: additional options dictionary
            :param lease: Lease to associate with this key.
            :type lease: int (ID of lease)
            :param prev_kv: return the previous key-value pair
            :type prev_kv: bool
        :returns: a response containing a header and other
        parameters
        """
        validparams = ['lease', 'prev_kv']
        if is_valid_params(validparams, **kwargs):
            return self.client.put(key, value, **kwargs)
        else:
            return None

    def get(self, key):
        """
        Get the value of a key from database.

        :param key: key in etcd to get
        :returns: value of key and metadata
        :rtype: bytes, ``Metadata``
        """
        return self.client.get(key)

    def get_prefix(self, key_prefix, **kwargs):
        """
        Get a range of keys with a prefix.

        :param key_prefix: first key in range
        :param kwargs: additional options dictionary
            :param sort_order: str,
            'None','ascend','descend'
            :param sort_target: str, 'key','value',
            'version','create','mod'
        :returns: sequence of (value, metadata) tuples
        """
        validparams = ['sort_order', 'sort_target']
        if is_valid_params(validparams, **kwargs):
            return self.client.get_prefix(key_prefix, **kwargs)
        else:
            return None

    def get_all(self, **kwargs):
        """
        Get all keys and values currently stored in the database.

        :param kwargs: additional options dictionary
            :param sort_order: str, 'None','ascend','descend'
            :param sort_target: str, 'key','value','version',
            'create','mod'
        :returns: sequence of (value, metadata) tuples
        """
        validparams = ['sort_order', 'sort_target']
        if is_valid_params(validparams, **kwargs):
            return self.client.get_all(**kwargs)
        else:
            return None

    def delete(self, key, **kwargs):
        """
        Delete a single key in database.

        :param key: key in database to delete

        :param kwargs: additional options dictionary
            :param prev_kv: return the deleted key-value pair
            :type prev_kv: bool
            :param return_response: return the full response
            :type return_response: bool
        :returns: True if the key has been deleted when
                  ``return_response`` is False and a response containing
                  a header, the number of deleted keys and prev_kvs when
                  ``return_response`` is True
        """
        validparams = ['prev_kv', 'return_response']
        if is_valid_params(validparams, **kwargs):
            return self.client.delete(key, **kwargs)
        else:
            return None

    def delete_prefix(self, prefix):
        """Delete a range of keys with a prefix in database.

        :param prefix: key prefix in database to delete
        :type prefix: str
        """
        return self.client.delete_prefix(prefix)

    def transaction(self, compare_ops, success_ops=None, failure_ops=None):
        """
        Perform a transaction.

        :param compare_ops: A list of comparisons to make
        :param success_ops: A list of operations to perform if all the comparisons
                        are true
        :param failure_ops: A list of operations to perform if any of the
                        comparisons are false
        :return: A tuple of (operation status, responses)
        """
        return self.client.transaction(compare_ops, success_ops, failure_ops)

    def add_watch_callback(self, *args, **kwargs):
        """
        Watch a key or range of keys and call a callback on every response.

        If timeout was declared during the client initialization and
        the watch cannot be created during that time the method raises
        a ``WatchTimedOut`` exception.

        :param key: key to watch
        :param callback: callback function

        :returns: watch_id. Later it could be used for cancelling watch.
        """
        return self.client.add_watch_callback(*args, **kwargs)

    def watch_callback(self, key_prefix, cb_fn, **kwargs):
        kwargs['range_end'] = etcd.utils.increment_last_byte(etcd.utils.to_bytes(key_prefix))

        return self.client.add_watch_callback(key=key_prefix, callback=cb_fn, **kwargs)

    def cancel_watch(self, watch_id):
        """
        Stop watching a key or range of keys.

        :param watch_id: watch_id returned by ``add_watch_callback`` method
        """
        return self.client.cancel_watch(watch_id)

    def lock(self, name, ttl=60):
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
        return self.client.lock(name, ttl)

    def lease(self, ttl, lease_id=None):
        """
        Create a new lease.

        All keys attached to this lease will be expired and deleted if the
        lease expires. A lease can be sent keep alive messages to refresh the
        ttl.

        :param ttl: Requested time to live
        :param lease_id: Requested ID for the lease

        :returns: new lease
        :rtype: :class:`.Lease`
        """
        return self.client.lease(ttl, lease_id)

    def get_lease_info(self, lease_id):
        """
        Get information about the lease.

        :param lease_id: Id of the lease to get information from.
        :type lease_id: int
        :returns information about the lease including remaining
        TTL in seconds.
        """
        return self.client.get_lease_info(lease_id)

    def revoke_lease(self, lease_id):
        """
        Revoke a lease.

        :param lease_id: ID of the lease to revoke.
        :type lease_id: int
        """
        return self.client.revoke_lease(lease_id)

    def members(self):
        """
        Return list of all members associated with the cluster.

        :rtype: sequence of :class:`.Member`
        """
        return self.client.members

    def status(self):
        """Get the status of the responding member."""
        return self.client.status()


def is_valid_init_args(**kwargs):
    """
    Validate the input options provided.
    """
    validparams = ['host', 'port', 'ca_cert', 'cert_key', 'cert_cert',
                   'timeout', 'user', 'password', 'options']

    return is_valid_params(validparams, **kwargs)


def is_valid_params(validparams, **kwargs):
    return ufmdb_client.is_valid_params(validparams, **kwargs)


def client(**kwargs):
    """
    Return an instance of Ufmdb_etcd3_Client.
    """
    return Ufmdb_etcd3_Client(**kwargs)
