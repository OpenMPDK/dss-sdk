import sys
import time
from common.ufmdb import ufmdb

#Call back function for monitoring DB changes
def db_watch_callback(event):
    for evnt in event.events:
        key = evnt.key.decode('utf-8')
        value = evnt.value.decode('utf-8')
        if 'Put' in str(evnt):
            print('\nTest 11: Callback notification received for Put event: key = {}, value = {}\n'.format(key, value))
        if 'Delete' in str(evnt):
            print('\nTest 11: Callback notification received for Delete event: key = {}\n'.format(key))

print('\nTest 1: **********Opening DB connection**********\n')
try:
    dbclient = ufmdb.client(db_type = 'etcd')
except Exception as e:
    print(e)
    sys.exit(1)

print('\nTest 2: **********Store a range of key-value pairs**********')
for i in range(5):
    key = '/key' + str(i)
    value = 'value' + str(i)
    print("Storing key = {}, value = {} in DB".format(key,value))
    dbclient.put(key,value)

print("\n")

print('\nTest 3: **********Retrieve a range of values**********')
for i in range(5):
    key = '/key' + str(i)
    (val,md) = dbclient.get(key)
    print("Retrieved {} for key {}".format(val.decode('utf-8'),key))

print('\nTest 4: **********Retrieving keys with a prefix /key **********')
for value, metadata in dbclient.get_prefix("/key"):
    print("Key: " + metadata.key.decode('utf-8'))
    print("Value: " + value.decode('utf-8'))

print('\nTest 5: **********Deleting key = {} in DB**********'.format(key))
dbclient.delete(key)

print('\nTest 6: **********Retrieving the value for key just deleted {}**********'.format(key))
(val,md) = dbclient.get(key)
if val != None:
    print("Retrieved {} for key {}\n".format(val.decode('utf-8'),key))
else:
    print("Retrieved {} for key {}\n".format(val,key))

dbclient.put('/newkey','newvalue')

print('\nTest 7: **********Retrieving all keys in the DB**********')
for value, metadata in dbclient.get_all():
    print("Key: " + metadata.key.decode('utf-8'))
    print("Value: " + value.decode('utf-8'))

print('\nTest 8: **********Deleting keys with a prefix /key **********')
dbclient.delete_prefix('/key')

print('\nTest 9: **********Retrieving all keys in the DB**********')
for value, metadata in dbclient.get_all():
    print("Key: " + metadata.key.decode('utf-8'))
    print("Value: " + value.decode('utf-8'))

#Test lease and related info
print('\nTest 10: *********Get Lease from DB*******')
lease = dbclient.lease(4, 1234)
lease_info = dbclient.get_lease_info(1234)
print(lease_info)

print('\nTest 11: **********Monitor changes for "/leasekey". Should receive callbacks**********')
watch_id = dbclient.add_watch_callback('/leasekey',db_watch_callback)

print('\nTest 12: **********Store and retrieve "/leasekey" and "leasevalue" pairs during and after the lease period**********')

#Store key for the lease
dbclient.put('/leasekey','leasevalue',lease = 1234)

#Retrieve the leasekey
(val,md) = dbclient.get('/leasekey')
print("Retrieved {} for key {}".format(val.decode('utf-8'),md.key.decode('utf-8')))

#Wait 5 seconds
print('\n**********Sleep for lease period of 5 seconds**********')
time.sleep(5)

#Check value for lease key after the lease period. The key should have been deleted
(val,md) = dbclient.get('/leasekey')
if val != None:
    print("Retrieved {} for key {}".format(val.decode('utf-8'),md.key.decode('utf-8')))
else:
    print("Key '/leasekey' not found")

print('\nTest 13: **********Revoke lease and check status**********')
dbclient.revoke_lease(1234)
lease_info = dbclient.get_lease_info(1234)
print(lease_info)

""" This function has known issue in getting into deadlock or not returning """
#dbclient.cancel_watch(watch_id)

print('\nTest 14: **********Get DB cluster member info**********')
for member in dbclient.members():
    print(member)

print('\nTest 15: **********Get DB cluster member status**********')
status = dbclient.status()
print('''
*******Status*******\n \
db_size: {}\n leader: {}\n raft_index: {}\n raft_term: {}\n
version: {}\n'''.format(status.db_size, status.leader,
                        status.raft_index, status.raft_term,
                        status.version))

print('\nTest 16: **********Test transaction**********')
print('\n**********If /transtest has value transvalue, store \'success\' else store \'failure\'**********')
dbclient.transaction(
    compare_ops=[
        dbclient.transactions.value('/transtest') == 'transvalue'
    ],
    success_ops=[
        dbclient.transactions.put('/transtest', 'success'),
    ],
    failure_ops=[
        dbclient.transactions.put('/transtest', 'failure'),
    ]
)

(val,md) = dbclient.get('/transtest')
if val != None:
    print("Retrieved {} for key {}".format(val.decode('utf-8'),md.key.decode('utf-8')))
else:
    print("key not found: /transtest\n")

print('\nTest 17: ************Test Lock acquire and release**********')
lock = dbclient.lock('lock1',3)
lock.acquire()
print('lock aquired')
lock.release()
print('lock released')

print('\n**********End of tests**********')
