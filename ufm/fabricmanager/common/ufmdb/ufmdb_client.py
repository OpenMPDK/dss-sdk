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


"""
Template class for database clients
"""

class UfmdbClient(object):
    def __init__(self, **kwargs):
        """Create connection to database."""
        self.client = None
        self.transactions = None
        print("ERROR: UfmdbClient.__init__ must be implemented!")

    def close(self):
        """Close connection to database."""
        print("ERROR: UfmdbClient.close must be implemented!")

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        self.close()

    def put(self, key, value, **kwargs):
        """
        Save a value to the database.

        :param key: key to set in database
        :param value: value to set key to
        :type value: bytes
        :param kwargs: additional options dictionary
        :returns: a response containing a header and other parameters
        """
        print("ERROR: UfmdbClient.put must be implemented!")
        return None

    def get(self, key, **kwargs):
        """
        Get the value of a key from database.

        :param key: key to get from database
        :param kwargs: additional options dictionary
        :returns: (value of key, metadata) tuple
        """
        print("ERROR: UfmdbClient.get must be implemented!")
        return None, None

    def get_prefix(self, key_prefix, **kwargs):
        """
        Get a range of keys with a prefix.

        :param key_prefix: first key in range
        :param kwargs: additional options dictionary
        :returns: sequence of (value, metadata) tuples
        """
        print("ERROR: UfmdbClient.get_prefix must be implemented!")
        return (None, None)

    def get_all(self, **kwargs):
        """
        Get all keys and values currently stored in the database.

        :returns: sequence of (value, metadata) tuples
        """
        print("ERROR: UfmdbClient.get_all must be implemented!")
        return (None, None)

    def delete(self, key, **kwargs):
        """
        Delete a single key in database.

        :param key: key in database to delete
        :param kwargs: additional options dictionary
        :returns: a response containing a header and other parameters
        """
        print("ERROR: UfmdbClient.delete must be implemented!")
        return None

    def delete_prefix(self, prefix):
        """Delete a range of keys with a prefix in database."""
        print("ERROR: UfmdbClient.delete_prefix must be implemented!")
        return None

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
        print("ERROR: UfmdbClient.transaction must be implemented!")
        return None, None

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
        print("ERROR: UfmdbClient.add_watch_callback must be implemented!")
        return None

    def cancel_watch(self, watch_id):
        """
        Stop watching a key or range of keys.

        :param watch_id: watch_id returned by ``add_watch_callback`` method
        """
        print("ERROR: UfmdbClient.cancel_watch must be implemented!")
        return None

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
        print("ERROR: UfmdbClient.lock must be implemented!")
        return None

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
        print("ERROR: UfmdbClient.lease must be implemented!")
        return None

    def get_lease_info(self, lease_id):
        """
        Returns time to live information about the lease.
        """
        print("ERROR: UfmdbClient.get_lease_info must be implemented!")
        return None

    def revoke_lease(self, lease_id):
        """
        Revoke a lease.

        :param lease_id: ID of the lease to revoke.
        """
        print("ERROR: UfmdbClient.revoke_lease must be implemented!")
        return None

    def members(self):
        """
        Return list of all members associated with the cluster.

        :rtype: sequence of :class:`.Member`
        """
        print("ERROR: UfmdbClient.members must be implemented!")
        return None

    def status(self):
        """Get the status of the responding member."""
        print("ERROR: UfmdbClient.status must be implemented!")
        return None

def is_valid_init_args(**kwargs):
    print("ERROR: UfmdbClient.is_valid_init_args must be implemented!")
    return True

def is_valid_params(validparams, **kwargs):
    for key in kwargs.keys():
        if key not in validparams:
            print("\"{}\" is not a valid option".format(key))
            return False
    return True

def client(**kwargs):
    return UfmdbClient(**kwargs)

def log_errors(func):
    def func_wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            print(e)
            return None
    return func_wrapper

