import importlib

from common.ufmlog import ufmlog
from functools import wraps

#Database module to use
db = None


class Ufmdb(object):
    def __new__(cls, **kwargs):
        #Get the database type, load the required module and
        #validate the inputs to the database connection.

        #Change the database to use based on the inputs
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

    def __init__(self,**kwargs):
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
        if (self.client != None):
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
    def lease(self, ttl, lease_id):
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

def client(**kwargs):
    """Return an instance of Ufmdb_Client."""
    return Ufmdb(**kwargs)
