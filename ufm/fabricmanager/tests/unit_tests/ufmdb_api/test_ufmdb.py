from common.ufmdb import ufmdb

import time
import pytest
import requests


@pytest.fixture(scope='module')
def db():
    print('----------login----------')

    db = ufmdb.client(db_type='etcd')

    yield db

    print('----------teardown----------')
    db.delete('/key_1')
    db.delete('/key_2')
    db.delete('/key_3')
    db.delete('/key_4')


def test_lease(db):

    lease_id_1 = 1001
    lease_obj = db.lease(10, lease_id_1)

    lease_info = db.get_lease_info(lease_id_1)
    print(lease_info)

    db.put('/key_1', 'value_1', lease=lease_id_1)
    db.put('/key_2', 'value_2', lease=lease_id_1)
    db.put('/key_3', 'value_3', lease=lease_id_1)
    db.put('/key_4', 'value_4', lease=lease_id_1)
    # db.put('/key_1', 'value_1')
    # db.put('/key_2', 'value_2')
    # db.put('/key_3', 'value_3')
    # db.put('/key_4', 'value_4')

    time.sleep(5)
    lease_info = db.get_lease_info(lease_id_1)
    print(lease_info)
    # lease_obj.refresh()
    lease_obj.LeaseKeepAliveRequest
    lease_info = db.get_lease_info(lease_id_1)
    print(lease_info)

    value, metadata = db.get('/key_1')
    print(type(value))
    assert(value == b'value_1')

    value, metadata = db.get('/key_2')
    assert(value == b'value_2')

    value, metadata = db.get('/key_3')
    assert(value == b'value_3')

    value, metadata = db.get('/key_4')
    assert(value == b'value_4')

    time.sleep(5)
    lease_info = db.get_lease_info(lease_id_1)
    print(lease_info)

    value, metadata = db.get('/key_1')
    print(type(value))
    assert(value == b'value_1')

    value, metadata = db.get('/key_2')
    assert(value == b'value_2')

    value, metadata = db.get('/key_3')
    assert(value == b'value_3')

    value, metadata = db.get('/key_4')
    assert(value == b'value_4')

    lease_id_2 = 1002
    lease_obj_2 = db.lease(10, lease_id_2)

    lease_info_2 = db.get_lease_info(lease_id_2)
    print(lease_info_2)

    db.put('/key_1', 'value_111', lease=lease_id_2)
    db.put('/key_2', 'value_222', lease=lease_id_2)
    db.put('/key_3', 'value_333', lease=lease_id_2)
    db.put('/key_4', 'value_444', lease=lease_id_2)
    value, metadata = db.get('/key_1')
    print(type(value))
    assert(value == b'value_111')

    value, metadata = db.get('/key_2')
    assert(value == b'value_222')

    value, metadata = db.get('/key_3')
    assert(value == b'value_333')

    value, metadata = db.get('/key_4')
    assert(value == b'value_444')
    '''
    lease_obj.refresh()
    '''
    time.sleep(15)
    lease_info_2 = db.get_lease_info(lease_id_2)
    print(lease_info_2)

    value, metadata = db.get('/key_1')
    assert(None is value)

    value, metadata = db.get('/key_2')
    assert(None is value)

    value, metadata = db.get('/key_3')
    assert(None is value)

    value, metadata = db.get('/key_4')
    assert(None is value)

    '''
    time.sleep(15)
    value, metadata = db.get('/key_1')
    print(type(value))
    assert(value == b'value_1')

    value, metadata = db.get('/key_2')
    assert(value == b'value_2')

    value, metadata = db.get('/key_3')
    assert(value == b'value_3')

    value, metadata = db.get('/key_4')
    assert(value == b'value_4')
    '''
