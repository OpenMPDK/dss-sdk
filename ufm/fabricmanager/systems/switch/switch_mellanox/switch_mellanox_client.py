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


import time
import json
import requests
from systems.switch import switch_constants
from systems.switch.switch_client import SwitchClientTemplate

'''
ignore warnings from this statement:
session.post( , , verify=False)
'''
import warnings
# warnings.filterwarnings("ignore", message="InsecureRequestWarning: Unverified HTTPS request is being made to host")
warnings.filterwarnings("ignore", message="Unverified HTTPS request is being made to host")


class SwitchMellanoxClient(SwitchClientTemplate):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = swArg.log
        self.url = None

        self.db = swArg.db
        self.lease_ttl = switch_constants.SWITCH_DB_KEY_TTL_SECS

        self.session = self._connect(swArg.usrname, swArg.pwd)
        self.uuid = self._poll_uuid()

        self.log.info("SwitchMellanoxClient ip = {}".format(self.swArg.sw_ip))
        self.log.info("Init {}".format(self.__class__.__name__))
        self.log.log_detail_on()

    def _connect(self, usrname, pwd):
        self.url = 'https://' + self.swArg.sw_ip + '/admin/launch'
        self.log.info('EthSwitch login url: ' + self.url)
        ses = requests.session()
        params = (
            ('script', 'rh'),
            ('template', 'login'),
            ('action', 'login'),
        )
        data = {
            'f_user_id': usrname,
            'f_password': pwd,
        }

        response = ses.post(self.url, params=params, data=data, verify=False)
        self.log.info('Switch connection status_code (200=OK): ' + str(response.status_code))
        # self.log.info('Switch response: %s', response.text)

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
        return self._poll_uuid()

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
        # if response.status_code != 200:
        # self.log.info('Switch response: %s', response.text)

        # A requests.models.Response can be returned as a tuple which provides extra info.
        # Such tuples have to be in the form (response, status, headers) where at least one
        # item has to be in the tuple. The status value will override the status code and
        # headers can be a list or dict of additional header values.
        #
        # return resp.text, resp.status_code, resp.headers.items()
        # return resp.content, resp.status_code, resp.headers.items()
        # return resp.raw.read(), resp.status_code, resp.headers.items()

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
                            "Ports": "Eth1/2, Eth1/3, Eth1/4, Eth1/5, Eth1/6,Eth1/7, Eth1/8, Eth1/9, Eth1/10, \
                                     Eth1/11,Eth1/12, Eth1/13, Eth1/14, Eth1/15, Eth1/16,Eth1/17, Eth1/18, \
                                     Eth1/19, Eth1/20, Eth1/21,Eth1/22, Eth1/23, Eth1/24, Eth1/25, Eth1/26, \
                                     Eth1/27, Eth1/28, Eth1/29, Eth1/30, Eth1/31,Eth1/32, Eth1/33, Eth1/34, \
                                     Eth1/35, Eth1/36,Eth1/37, Eth1/38, Eth1/39, Eth1/40, Eth1/41,Eth1/42, \
                                     Eth1/43, Eth1/44, Eth1/45, Eth1/46,Eth1/47, Eth1/48, Eth1/51, Eth1/52, Eth1/56"
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

    def show_pfc_status(self):
        '''
        {
            'results': [
                {
                    'status': 'OK',
                    'executed_command': 'show dcb priority-flow-control',
                    'status_message': '',
                    'data': [
                        {
                        'PFC': 'enabled',
                        'Priority Enabled List': '3 4',
                        'Priority Disabled List': '0 1 2 5 6 7'
                        },
                        {
                        'Eth1/11': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/10': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/13': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/12': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/15': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/14': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/16': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Enabled'}
                            ],
                        'Eth1/5': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/4': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/7': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/6': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/1': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/3': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/2': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/9': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ],
                        'Eth1/8': [
                            {
                            'PFC admin': 'Auto',
                            'PFC oper': 'Disabled'}
                            ]
                        }
                    ]
                }
            ]
        }
        '''
        json_cmd = {
            "commands":
            [
                "show dcb priority-flow-control"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def show_qos(self):
        '''
        {'results': [
            {
            'status': 'OK',
            'executed_command': 'show qos',
            'status_message': '',
            'data': [
                 {
                 'Eth1/1': [
                     {
                     'PCP,DEI rewrite': 'disabled',
                     'Default switch-priority': '0',
                     'IP PCP;DEI rewrite': 'enable',
                     'Default DEI': '0',
                     'Default PCP': '0',
                     'Trust mode': 'L2',
                     'DSCP rewrite': 'disabled'
                     },
                     ......
                    ]
                },
                {
                'Eth1/2': [
        '''
        json_cmd = {
            "commands":
            [
                # "show qos interface ethernet 1/" + str(port_id)
                "show qos"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def _poll_qos(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/8/pfc/trust_mode/L2
        '''
        resp = self.show_qos()
        json_obj = resp.json()

        '''
        Parsing the response from show_qos cmd
        '''
        if 'results' in json_obj:
            if len(json_obj['results']) > 0:
                if 'executed_command' in json_obj['results'][0] and 'data' in json_obj['results'][0]:
                    if json_obj['results'][0]['executed_command'] == 'show qos':
                        SWITCH_PORT_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/ports/'
                        lease = self.db.lease(self.lease_ttl)

                        data = json_obj['results'][0]['data']  # a list of dictionaries
                        for d in data:
                            for k in d:
                                if 'Eth1/' in k:
                                    pt = k.split('/')[-1]
                                    self.db.put(SWITCH_PORT_KEY_PREFIX + str(pt) + '/pfc/trust_mode/'
                                                + d[k][0]['Trust mode'], '', lease=lease)

    def _poll_pfc_status(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/pfc_status/enabled
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_enabled_list/3
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_enabled_list/4
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/0
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/2
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/5
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/6
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/7

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/8/pfc/pfc_status/disabled
        '''
        resp = self.show_pfc_status()
        json_obj = resp.json()

        '''
        Parsing the response from show_pfc_status cmd
        '''
        if 'results' in json_obj:
            if len(json_obj['results']) > 0:
                if 'executed_command' in json_obj['results'][0] and 'data' in json_obj['results'][0]:
                    if json_obj['results'][0]['executed_command'] == 'show dcb priority-flow-control':
                        SWITCH_ATTR_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/switch_attributes'
                        SWITCH_PORT_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/ports/'
                        lease = self.db.lease(self.lease_ttl)

                        data = json_obj['results'][0]['data']  # a list of dictionaries
                        for d in data:
                            for k in d:
                                if k == 'PFC':
                                    self.db.put(SWITCH_ATTR_KEY_PREFIX + '/pfc/pfc_status/' + d['PFC'], '', lease=lease)

                                elif k == 'Priority Enabled List':
                                    if d['Priority Enabled List']:
                                        lst = d['Priority Enabled List'].split(' ')
                                        for i in lst:
                                            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/pfc/priority_enabled_list/' + str(i),
                                                        '', lease=lease)

                                elif k == 'Priority Disabled List':
                                    if d['Priority Disabled List']:
                                        lst = d['Priority Disabled List'].split(' ')
                                        for i in lst:
                                            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/pfc/priority_disabled_list/'
                                                        + str(i), '', lease=lease)

                                elif 'Eth1/' in k:
                                    pt = k.split('/')[-1]
                                    self.db.put(SWITCH_PORT_KEY_PREFIX + str(pt) + '/pfc/pfc_status/'
                                                + d[k][0]['PFC oper'], '', lease=lease)

    def _poll_switch_attributes(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/ipv4/10.1.10.191
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/manufacturer/mellanox
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/model/x86onie
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/name/default
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/serial_number/MT1935J01956
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/uptime/1591390118
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/uuid/f1ec15f8-c832-11e9-8000-b8599f784980
        /switches/list/f1ec15f8-c832-11e9-8000-b8599f784980
        '''
        resp = self.show_version()
        json_obj = resp.json()

        '''
        Parsing the response from show_version cmd
        '''
        if 'results' in json_obj:
            if len(json_obj['results']) > 0:
                if 'executed_command' in json_obj['results'][0] and 'data' in json_obj['results'][0]:

                    if json_obj['results'][0]['executed_command'] == 'show version':
                        data = json_obj['results'][0]['data']
                        if 'System UUID' in data:
                            lease = self.db.lease(self.lease_ttl)

                            self.uuid = data['System UUID']
                            self.db.put(switch_constants.SWITCH_LIST_KEY_PREFIX + '/' + self.uuid, '', lease=lease)
                            SWITCH_ATTR_KEY_PREFIX = \
                                switch_constants.SWITCH_BASE + '/' + self.uuid + '/switch_attributes'

                            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/uptime/' + str(int(time.time())), '', lease=lease)
                            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/uuid/' + self.uuid, '', lease=lease)
                            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/manufacturer/' + self.swArg.sw_type.lower(),
                                        '', lease=lease)
                            self.db.put(SWITCH_ATTR_KEY_PREFIX + '/ipv4/' + self.swArg.sw_ip, '', lease=lease)

                            if 'Name' in data:
                                self.db.put(SWITCH_ATTR_KEY_PREFIX + '/name/' + data['Name'], '', lease=lease)
                            if 'Product model' in data:
                                self.db.put(SWITCH_ATTR_KEY_PREFIX + '/model/' + data['Product model'], '', lease=lease)
                            if 'System serial num' in data:
                                self.db.put(SWITCH_ATTR_KEY_PREFIX + '/serial_number/' + data['System serial num'],
                                            '', lease=lease)

    def _poll_vlan_info(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/1/name/default
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/100/name/
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/100/network/ports/53
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/100/network/ports/55
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/name/
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/network/ports/49
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/network/ports/50
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/101/network/ports/54
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/6/name/
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/6/network/ports/1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/100
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/101
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/VLANs/list/6
        '''
        resp = self.show_vlan()
        json_obj = resp.json()

        '''
        Parsing response from show_vlan cmd
        '''
        if 'results' in json_obj:
            if len(json_obj['results']) > 0:
                if 'executed_command' in json_obj['results'][0] and 'data' in json_obj['results'][0]:

                    if json_obj['results'][0]['executed_command'] == 'show vlan':

                        for vlan_id, vlan_info in json_obj['results'][0]['data'].items():
                            lease = self.db.lease(self.lease_ttl)

                            VLAN_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/VLANs'
                            self.db.put(VLAN_KEY_PREFIX + '/list/' + vlan_id, '', lease=lease)

                            this_vlan_key_prefix = VLAN_KEY_PREFIX + '/' + vlan_id
                            for k, v in vlan_info[0].items():
                                if k == 'Name':
                                    self.db.put(this_vlan_key_prefix + '/name/' + v, '', lease=lease)
                                elif k == 'Ports':
                                    for port_id in [x.strip() for x in v.split(',')]:  # split and strip whitespace
                                        port_id = port_id.split('/')[-1]
                                        if port_id:
                                            self.db.put(this_vlan_key_prefix + '/network/ports/' + port_id,
                                                        '', lease=lease)

    def _poll_port_info(self):
        '''
        Poll from the switch and add to db:

        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/list/52
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/list/53
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/list/54
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/access
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/access_vlan/1
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/allowed_vlans/
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/53/mode/trunk
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/53/network/access_vlan/
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/53/network/allowed_vlans/100
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/54/mode/trunk
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/54/network/access_vlan/
        /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/54/network/allowed_vlans/101
        '''
        resp = self.show_port()
        json_obj = resp.json()

        '''
        Parsing repsonse from show_port
        '''
        if 'results' in json_obj:
            if len(json_obj['results']) > 0:
                if 'executed_command' in json_obj['results'][0] and 'data' in json_obj['results'][0]:

                    if json_obj['results'][0]['executed_command'] == 'show interfaces switchport':
                        for port_id, port_info in json_obj['results'][0]['data'].items():
                            lease = self.db.lease(self.lease_ttl)

                            port_id = port_id.split('/')[-1]
                            PORT_KEY_PREFIX = switch_constants.SWITCH_BASE + '/' + self.uuid + '/ports'
                            self.db.put(PORT_KEY_PREFIX + '/list/' + port_id, '', lease=lease)

                            this_port_key_prefix = PORT_KEY_PREFIX + '/' + port_id
                            for k, v in port_info[0].items():
                                if k == 'Mode':
                                    self.db.put(this_port_key_prefix + '/mode/' + v, '', lease=lease)
                                elif k == 'Access vlan':
                                    if v != 'N/A':
                                        # Note: some port's Access vlan is N/A, different from empty?
                                        self.db.put(this_port_key_prefix + '/network/access_vlan/' + v, '', lease=lease)
                                elif k == 'Allowed vlans':
                                    for allowed_id in [x.strip() for x in v.split(',')]:  # split and strip whitespace
                                        self.db.put(this_port_key_prefix + '/network/allowed_vlans/' + v,
                                                    '', lease=lease)

    def poll_to_db(self):
        # For now (06/2020), Redfish Fabric exists only for Switch. May change in the future.
        self.db.put('/Fabrics/list/Fabric.1', '', lease=self.db.lease(self.lease_ttl))
        self.db.put('/Fabrics/Fabric.1/type/NVME', '', lease=self.db.lease(self.lease_ttl))
        self.db.put('/Fabrics/Fabric.1/list/' + self.uuid, '', lease=self.db.lease(self.lease_ttl))

        self._poll_switch_attributes()
        self._poll_vlan_info()
        self._poll_port_info()
        self._poll_pfc_status()
        self._poll_qos()

        try:
            # Write in the mq_port for UfmRedfish service to fetch
            self.db.put('/switches/' + self.uuid + '/switch_attributes/mq_port/' + str(self.swArg.port), '',
                        lease=self.db.lease(self.lease_ttl))
        except Exception as e:
            print(e)

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
                "vlan " + str(vlan_id),
                "exit"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def name_vlan(self, vlan_id, name):
        json_cmd = {
            "commands":
            [
                "vlan " + str(vlan_id) + " name " + name
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def set_access_port_vlan(self, port_id, vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " switchport mode access",
                "interface ethernet 1/" + str(port_id) + " switchport access vlan " + str(vlan_id),
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def unassign_access_port_vlan(self, port_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " no switchport access vlan"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def set_trunk_port_vlans_all(self, port_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " switchport mode trunk",
                "interface ethernet 1/" + str(port_id) + " switchport trunk allowed-vlan all"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def set_trunk_port_vlans_range(self, port_id, start_vlan_id, end_vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " switchport mode trunk",
                "interface ethernet 1/" + str(port_id) +
                " switchport trunk allowed-vlan " + str(start_vlan_id) + "-" + str(end_vlan_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def set_hybrid_port_access_vlan(self, port_id, access_vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " switchport mode hybrid",
                "interface ethernet 1/" + str(port_id) + " switchport access vlan " + str(access_vlan_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def set_hybrid_port_allowed_vlan(self, port_id, allowed_vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " switchport mode hybrid",
                "interface ethernet 1/" + str(port_id) +
                " switchport hybrid allowed-vlan add " + str(allowed_vlan_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def remove_hybrid_port_allowed_vlan(self, port_id, vlan_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) +
                " switchport hybrid allowed-vlan remove " + str(vlan_id)
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
        resp = self.send_cmd(json_cmd)
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
        resp = self.send_cmd(json_cmd)
        return resp

    '''
    PFC config (proiroty flow control)
    '''
    def enable_pfc_globally(self):
        json_cmd = {
            "commands":
            [
                "dcb priority-flow-control enable force"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def disable_pfc_globally(self):
        json_cmd = {
            "commands":
            [
                "no dcb priority-flow-control enable force",
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def enable_pfc_per_priority(self, prio):
        json_cmd = {
            "commands":
            [
                "dcb priority-flow-control priority " + str(prio) + " enable",
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def disable_pfc_per_priority(self, prio):
        json_cmd = {
            "commands":
            [
                "no dcb priority-flow-control priority " + str(prio) + " enable"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def enable_port_pfc(self, port_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " dcb priority-flow-control mode on force"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def disable_port_pfc(self, port_id):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " no dcb priority-flow-control mode force"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def show_port_pfc_counters(self, port_id, prio):
        json_cmd = {
            "commands":
            [
                "show interfaces ethernet 1/" + str(port_id) + " counters pfc prio " + str(prio)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def enable_ecn_marking_for_traffic_class_queue(self, port_id, tc, min_absolute, max_absolute):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " traffic-class " + str(tc) +
                " congestion-control ecn minimum-absolute " + str(min_absolute) +
                " maximum-absolute " + str(max_absolute)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def disable_ecn_marking_for_traffic_class_queue(self, port_id, tc):
        json_cmd = {
            "commands":
            [
                "interface ethernet 1/" + str(port_id) + " no traffic-class " + str(tc) + " congestion-control"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def show_port_congestion_control(self, port_id):
        json_cmd = {
            "commands":
            [
                'show interface ethernet 1/' + str(port_id) + ' congestion-control'
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def any_cmd(self, any_cmd_str):
        json_cmd = {
            "commands":
            [
                any_cmd_str
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def save_configuration_file_no_switch(self, file_name):
        json_cmd = {
            "commands":
            [
                'configuration write to ' + file_name + ' no-switch'
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def show_configuration_files(self):
        json_cmd = {
            "commands":
            [
                'show configuration files'
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def enable_buffer_management(self):
        json_cmd = {
            "commands":
            [
                'advanced buffer management force'
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def disable_buffer_management(self):
        json_cmd = {
            "commands":
            [
                'no advanced buffer management force'
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def show_port_buffer_details(self, port_id):
        json_cmd = {
            "commands":
            [
                'show buffers details interfaces ethernet 1/' + str(port_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def bind_port_priority_to_specific_buffer(self, port_id, buf, prio):
        json_cmd = {
            "commands":
            [
                'interface ethernet 1/' + str(port_id) + ' ingress-buffer ' + buf + ' bind switch-priority ' + str(prio)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp
