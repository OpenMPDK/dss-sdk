from systems.switch import switch_controller
from common.ufmlog import ufmlog

import pytest
import requests

MELLANOX_SWITCH_TYPE = 'mellanox'
MELLANOX_SWITCH_IP = '10.1.10.191'

@pytest.fixture(scope='module')
def sw():
    print('----------login----------')
    ufmlog.ufmlog.filename = "/tmp/test_switch.log"
    #print(ufmlog.ufmlog.filename)
    log = ufmlog.log(module="switch_test", mask=ufmlog.UFM_SWITCH)
    log.log_detail_on()
    log.log_debug_on()
    print(log)

    sw = switch_controller.client(MELLANOX_SWITCH_TYPE, MELLANOX_SWITCH_IP, log)
    sw.start()
    yield sw

    print('----------teardown----------')
    sw.stop()


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

def test_config_access_mode_and_assign_pvid(sw):
    vlan_id = 7
    port = '1/31'

    #sw.enter_config_cmd()
    sw.config_access_mode_and_assign_pvid(vlan_id, port)

    data = {
        "commands":
        [
            "show vlan id " + str(vlan_id),
        ]
    }
    resp = sw.send_cmd(data)

    json_obj = resp.json()
    assert(json_obj["results"][0]["executed_command"] == "show vlan id 7")
    assert(json_obj["results"][0]["status"] == "OK")
    assert(json_obj["results"][0]["data"]["7"][0]["Ports"] == "Eth1/31")

    #delete port and vlan for next run
    data1 = {
        "commands":
        [
            "interface ethernet " + port,
            "no switchport access vlan",
            "exit",
            "no vlan " + str(vlan_id)
        ]
    }
    resp = sw.send_cmd(data1)
    assert(resp.status_code == 200)

    json_obj = resp.json()
    assert(json_obj["results"][0]["executed_command"] == "interface ethernet 1/31")
    assert(json_obj["results"][0]["status"] == "OK")
    assert(json_obj["results"][1]["executed_command"] == "no switchport access vlan")
    assert(json_obj["results"][1]["status"] == "OK")
    assert(json_obj["results"][2]["executed_command"] == "exit")
    assert(json_obj["results"][2]["status"] == "OK")
    assert(json_obj["results"][3]["executed_command"] == "no vlan 7")
    assert(json_obj["results"][3]["status"] == "OK")


