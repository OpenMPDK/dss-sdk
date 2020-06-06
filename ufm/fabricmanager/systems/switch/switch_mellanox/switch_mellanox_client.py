


'''
ignore warnings from this statement:
session.post( , , verify=False)
'''
import warnings
warnings.filterwarnings("ignore", message="InsecureRequestWarning: Unverified HTTPS request is being made to host")

import time
import json
import requests
from systems.switch import switch_constants
from systems.switch.switch_arg import SwitchArg
from systems.switch.switch_client import SwitchClientTemplate

class SwitchMellanoxClient(SwitchClientTemplate):
    def __init__(self, swArg):
        self.swArg = swArg
        self.db = swArg.db
        self.log = swArg.log
        self.url = None

        self.session = self._connect()
        self.uuid = self._poll_uuid()

        self.log.info("SwitchMellanoxClient ip = {}".format(self.swArg.sw_ip))
        self.log.info("Init {}".format(self.__class__.__name__))


    def _connect(self):
        self.url = 'https://' + self.swArg.sw_ip + '/admin/launch'
        self.log.info('EthSwitch login url: ' + self.url)
        ses = requests.session()
        params = (
            ('script', 'rh'),
            ('template', 'login'),
            ('action', 'login'),
        )
        data = {
            'f_user_id': 'admin',
            'f_password': 'admin',
        }

        response = ses.post(self.url, params=params, data=data, verify=False)
        self.log.info('Switch connection status_code (200=OK): ' + str(response.status_code))
        #self.log.info('Switch response: %s', response.text)

        return ses

    def _poll_uuid(self):
        resp = self.show_version()
        json_obj = resp.json()

        if json_obj['results'][0]['executed_command'] == 'show version':
            return json_obj['results'][0]['data']['System UUID']
        else:
            return None

    def get_uuid(self):
        if self.uuid:
            return self.uuid
        return _poll_uuid()

    def send_cmd(self, json_data):
        '''
        example_data = {
            "commands":
            [
                "show version", # can keep adding lines
            ]
        }
        '''
        self.log.info('Sending EthSwitch cmd: ')
        self.log.info(json.dumps(json_data))

        response = self.session.post(self.url + '?script=json', json=json_data, verify=False)

        self.log.info('Switch response status_code (200=OK): ' + str(response.status_code))
        self.log.info('Switch response: %s', response.text)
        return response


    def enable_cmd(self):
        json_cmd = {
            "commands":
            [
                "enable",
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp


    def show_version(self):
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
        json_cmd = {
            "commands":
            [
                "show version",
            ]
        }

        resp = self.send_cmd(json_cmd)
        return resp

    def show_vlan(self):
        '''
        {
            "results": [
                {
                "status": "OK",
                "executed_command": "show vlan",
                "status_message": "",
                "data": {
                    "1": [
                        {
                            "Name": "default",
                            "Ports": "Eth1/2, Eth1/3, Eth1/4, Eth1/5, Eth1/6,Eth1/7, Eth1/8, Eth1/9, Eth1/10, Eth1/11,Eth1/12, Eth1/13, Eth1/14, Eth1/15, Eth1/16,Eth1/17, Eth1/18, Eth1/19, Eth1/20, Eth1/21,Eth1/22, Eth1/23, Eth1/24, Eth1/25, Eth1/26,Eth1/27, Eth1/28, Eth1/29, Eth1/30, Eth1/31,Eth1/32, Eth1/33, Eth1/34, Eth1/35, Eth1/36,Eth1/37, Eth1/38, Eth1/39, Eth1/40, Eth1/41,Eth1/42, Eth1/43, Eth1/44, Eth1/45, Eth1/46,Eth1/47, Eth1/48, Eth1/51, Eth1/52, Eth1/56"
                        }
                        ],
                        "100": [
                        {
                            "Name": "",
                            "Ports": "Eth1/53, Eth1/55"
                        }
                        ],
                        "101": [
                        {
                            "Name": "",
                            "Ports": "Eth1/49, Eth1/50, Eth1/54"
                        }
                        ],
                        "6": [
                        {
                            "Name": "",
                            "Ports": "Eth1/1"
                        }
                        ]
                    }
                }
            ]
        }

        '''
        json_cmd = {
            "commands":
            [
                "show vlan",
            ]
        }

        resp = self.send_cmd(json_cmd)
        return resp

    def show_port(self):
        '''
        {
            "results": [
                {
                    "status": "OK",
                    "executed_command": "show interfaces switchport",
                    "status_message": "",
                    "data": {
                        "Eth1/30": [
                        {
                        "Access vlan": "1",
                        "Allowed vlans": "",
                        "Mode": "access"
                        }
                    ],
                        "Eth1/55": [
                        {
                        "Access vlan": "100",
                        "Allowed vlans": "",
                        "Mode": "access"
                        }
                    ],
                        "Eth1/54": [
                        {
                        "Access vlan": "N/A",
                        "Allowed vlans": "101",
                        "Mode": "trunk"
                        }
                    ],
                        "Eth1/56": [
                        {
                        "Access vlan": "1",
                        "Allowed vlans": "",
                        "Mode": "access"
                        }
                    ],
                        "Eth1/51": [
                        {
                        "Access vlan": "1",
                        "Allowed vlans": "",
                        "Mode": "access"
                        }
                    ],
                        "Eth1/50": [
                        {
                        "Access vlan": "N/A",
                        "Allowed vlans": "101",
                        "Mode": "trunk"
                        }
                    ],
                        "Eth1/53": [
                        {
                        "Access vlan": "N/A",
                        "Allowed vlans": "100",
                        "Mode": "trunk"
                        }
                    ],
                    ......
                    ......
                    ......
                    }
                }
            ]
        }
        '''
        json_cmd = {
            "commands":
            [
                "show interfaces switchport",
            ]
        }

        resp = self.send_cmd(json_cmd)
        return resp

    def _poll_switch_attributes(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/ipv4
        10.1.10.191
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/manufacturer
        mellanox
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/model
        x86onie
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/serial_number
        MT1935J01956
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/uptime
        1591390118
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/uuid
        f1ec15f8-c832-11e9-8000-b8599f784980
        /switches/list/f1ec15f8-c832-11e9-8000-b8599f784980
        '''
        resp = self.show_version()
        json_obj = resp.json()

        if json_obj['results'][0]['executed_command'] == 'show version':
            self.uuid = json_obj['results'][0]['data']['System UUID']
            self.db.put(switch_constants.SWITCH_LIST_KEY_PREFIX + '/' + self.uuid, "")

            SWITCH_ATTR_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/switch_attributes'
            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/manufacturer', self.swArg.sw_type.lower())
            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/model', json_obj['results'][0]['data']['Product model'])
            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/serial_number', json_obj['results'][0]['data']['System serial num'])
            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/uuid', self.uuid)
            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/ipv4', self.swArg.sw_ip)
            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/uptime', str(int(time.time())))


    def _poll_vlan_info(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/100/name
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/100/network/ports/Eth1/53
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/100/network/ports/Eth1/55
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/name
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/network/ports/Eth1/49
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/network/ports/Eth1/50
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/network/ports/Eth1/54
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/6/name
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/6/network/ports/Eth1/1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/100
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/101
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/6
        '''
        resp = self.show_vlan()
        json_obj = resp.json()
        if json_obj['results'][0]['executed_command'] == 'show vlan':
            for vlan_id, vlan_info in json_obj['results'][0]['data'].items():
                VLAN_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/VLANs'

                self.db.put(VLAN_KEY_PREFIX + '/list/' + vlan_id, "")

                this_vlan_key_prefix = VLAN_KEY_PREFIX + '/' + vlan_id
                for k, v in vlan_info[0].items():
                   if k == 'Name':
                       self.db.put(this_vlan_key_prefix + '/name', v)
                   elif k == 'Ports':
                       for port_id in [x.strip() for x in v.split(',')]:#split and strip whitespace
                           self.db.put(this_vlan_key_prefix + '/network/ports/' + port_id, "")


    def _poll_port_info(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/52/mode
        access
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/52/network/access_vlan
        1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/52/network/allowed_vlans

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/53/mode
        trunk
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/53/network/access_vlan

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/53/network/allowed_vlans
        100
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/54/mode
        trunk
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/54/network/access_vlan

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/Eth1/54/network/allowed_vlans
        101
        '''
        resp = self.show_port()
        json_obj = resp.json()

        if json_obj['results'][0]['executed_command'] == 'show interfaces switchport':
            for port_id, port_info in json_obj['results'][0]['data'].items():
                PORT_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/ports'

                self.db.put(PORT_KEY_PREFIX + '/list/' + port_id, "")

                this_port_key_prefix = PORT_KEY_PREFIX + '/' + port_id
                for k, v in port_info[0].items():
                   if k == 'Mode':
                       self.db.put(this_port_key_prefix + '/mode', v)
                   elif k == 'Access vlan':
                       #Note: some port's Access vlan is N/A, different from empty?
                       self.db.put(this_port_key_prefix + '/network/access_vlan', v)
                   elif k == 'Allowed vlans':
                       for allowed_id in [x.strip() for x in v.split(',')]:#split and strip whitespace
                           self.db.put(this_port_key_prefix + '/network/allowed_vlans', v)



    def poll_to_db(self):
        self._poll_switch_attributes()
        self._poll_vlan_info()
        self._poll_port_info()



    def delete_vlan(self, vlan_id):
        json_cmd = {
            "commands":
            [
                "no vlan " + str(vlan_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp


    def create_vlan(self, vlan_id):
        json_cmd = {
            "commands":
            [
                "vlan " + str(vlan_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def associate_ip_to_vlan(self, vlan_id, ip_address):
        json_cmd = {
            "commands":
            [
                "interface vlan " + str(vlan_id),
                "ip address " + ip_address,
                "exit"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp

    def remove_ip_from_vlan(self, vlan_id, ip_address):
        json_cmd = {
            "commands":
            [
                "interface vlan " + str(vlan_id),
                "no ip address " + ip_address,
                "exit"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp



    def assign_port_to_vlan(self, port, vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet "+ port,
                "switchport access vlan " + str(vlan_id),
                "exit"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp

    def add_mode_port_to_vlan(self, port, mode, vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet " + port,
                "switchport mode " + mode,
                "switchport " + mode + " allowed-vlan add " + str(vlan_id),
                "exit"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp



