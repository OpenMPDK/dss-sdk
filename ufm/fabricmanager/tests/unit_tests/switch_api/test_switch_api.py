
from common.ufmlog import ufmlog
from common.ufmdb import ufmdb
from systems.switch import switch_constants
from systems.switch.switch_arg import SwitchArg
from systems.switch.switch_mellanox.switch_mellanox_client import SwitchMellanoxClient

import pytest
import requests
import time

MELLANOX_SWITCH_TYPE = 'mellanox'
MELLANOX_SWITCH_IP = '10.1.10.191'
MELLANOX_UUID = 'f1ec15f8-c832-11e9-8000-b8599f784980'

db = ufmdb.client(db_type = 'etcd')

@pytest.fixture(scope='module')
def sw():
    print('----------login----------')

    ufmlog.ufmlog.filename = "/tmp/test_switch.log"
    #print(ufmlog.ufmlog.filename)
    log = ufmlog.log(module="switch_test", mask=ufmlog.UFM_SWITCH)
    log.log_detail_on()
    log.log_debug_on()
    print(log)

    global db
    swArg = SwitchArg(sw_type = MELLANOX_SWITCH_TYPE,
                      sw_ip = MELLANOX_SWITCH_IP,
                      log = log,
                      db = db)

    sw = SwitchMellanoxClient(swArg)
    yield sw

    print('----------teardown----------')
    db.client.delete_prefix(switch_constants.SWITCH_LIST_KEY_PREFIX)
    db.client.delete_prefix(switch_constants.SWITCH_BASE + '/' + MELLANOX_UUID)

def test_show_version(sw):
    '''
    expected = {
        "results": [
            {
                "status": "OK",
                "executed_command": "show version",
                "status_message": "",
                "data": {
                    "Uptime": "34d 20h 12m 48.256s",
                    "CPU load averages": "3.19 / 3.32 / 3.26",
                    "Build date": "2020-01-29 11:48:15",
                    "Target arch": "x86_64",
                    "Target hw": "x86_64",
                    "Number of CPUs": "2",
                    "Build ID": "#1-dev",
                    "Host ID": "B8599FBE8E56",
                    "System serial num": "MT1935J01956",
                    "System UUID": "f1ec15f8-c832-11e9-8000-b8599f784980",
                    "Swap": "0 MB used / 0 MB free / 0 MB total",
                    "Product name": "Onyx",
                    "Built by": "jenkins@51fc8996eccd",
                    "System memory": "2480 MB used / 5309 MB free / 7789 MB total",
                    "Product model": "x86onie",
                    "Product release": "3.8.2306",
                    "Version summary": "X86_64 3.8.2306 2020-01-29 11:48:15 x86_64"
                }
            }
        ]
    }
    '''
    data = {
        "commands":
        [
            "show version",
        ]
    }
    resp = sw.send_cmd(data)
    assert('Product name' in resp.text)
    assert('Onyx' in resp.text)
    assert('System serial num' in resp.text)
    assert('System UUID' in resp.text)

    json_obj = resp.json()
    assert(json_obj["results"][0]["executed_command"] == "show version")
    assert(json_obj["results"][0]["status"] == "OK")


def verify_db():
    ret_uuid_list = []
    ret_vlan_list = []

    global db
    for value, md in db.get_prefix(switch_constants.SWITCH_LIST_KEY_PREFIX):
        key_str = (md.key).decode('utf-8')
        key_list = key_str.split('/')
        ret_uuid = key_list[-1]
        ret_uuid_list.append(ret_uuid)

    assert(MELLANOX_UUID in ret_uuid_list)

    # more direct way for above
    key = switch_constants.SWITCH_LIST_KEY_PREFIX + '/' + MELLANOX_UUID
    value, md = db.get(key)
    assert(md.key.decode('utf-8') == key)
    ''' 
    # There is always a vlan '1' with name 'default'
    key = switch_constants.SWITCH_BASE + '/' + MELLANOX_UUID + '/VLANs/list'
    for value, md in db.get_prefix(key):
        key_str = (md.key).decode('utf-8')
        key_list = key_str.split('/')
        ret_vlan_id = key_list[-1]
        ret_vlan_list.append(ret_vlan_id)

    assert( '1' in ret_vlan_list)

    # more direct way for above
    key = switch_constants.SWITCH_BASE + '/' + MELLANOX_UUID + '/VLANs/list/1'
    value, md = db.get(key)
    assert( md.key.decode('utf-8') == key)

 
    key = switch_constants.SWITCH_BASE + '/' + MELLANOX_UUID + '/VLANs/1/name/default'
    value, md = db.get(key)
    assert( md.key.decode('utf-8') == key)
    ''' 

def test_poll_to_db(sw):

    count = 5
    while count > 0:
        count -= 1

        sw.poll_to_db()
        verify_db()

        time.sleep(switch_constants.SWITCH_POLLER_INTERVAL_SECS)
        verify_db()


    

