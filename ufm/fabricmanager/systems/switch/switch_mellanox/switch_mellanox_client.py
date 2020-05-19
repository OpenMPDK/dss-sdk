
import json
import requests
from systems.switch.switch_controller import SwitchController

class SwitchMellanoxClient(SwitchController):
    def __init__(self, sw_type, ip_address, log=None, db=None, mq=None):
        self.ip_address = ip_address
        self.log = log
        self.db = db
        self.mq = mq
        self.url = 'https://'
        self.log.info("SwitchController ip_address = {}".format(self.ip_address))
        self.running = False
        self.session = None


    def __del__(self):
        pass

    def start(self):
        self.log.info("======> SwitchController <=========")

        if self.running:
            self.log.debug("==> SwitchController is already running <==")
            return

        self.session = self.connect()
        self.running = True
        self.log.info("======> SwitchController has started <=========")


    def stop(self):
        self.running = False
        self.log.info("======> NkvMonitor Stopped <=========")


    def is_running(self):
        return self.running

    def connect(self):
        self.url += self.ip_address + '/admin/launch'
        self.log.info('Switch login url: ' + self.url)
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
        print(str(response.status_code))
        self.log.info('Switch response status_code (200=OK): ' + str(response.status_code))
        self.log.info('Switch response: %s', response.text)

        # hardcoded the future cmds to be json format
        self.url = self.url + '?script=json'
        self.log.info('Switch json cmd url: ' + self.url)

        return ses


    def send_cmd(self, json_data):
        '''
        example_data = {
            "commands":
            [
                "show version", # can keep adding lines
            ]
        }
        '''
        self.log.info('Switch cmd url: ' + self.url)

        self.log.info('Sending Switch cmd: ')
        self.log.info(json.dumps(json_data))

        response = self.session.post(self.url, json=json_data, verify=False)

        self.log.info('Switch response status_code (200=OK): ' + str(response.status_code))
        self.log.info('Switch response: %s', response.text)
        return response




    '''
    Basic Commands
    '''
    def enter_config_cmd(self):
        json_cmd = {
            "commands":
            [
                "enable",
                "configure terminal"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def back_to_config_mode(self):
        json_cmd = {
            "commands":
            [
                "exit"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def no_vlan(self, vlan_id):
        json_cmd = {
            "commands":
            [
                "no vlan " + str(vlan_id)
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def show_interface_vlan(self, vlan_id):
        json_cmd = {
            "commands":
            [
                "show interface vlan " + str(vlan_id)
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp

    def show_interface_ethernet_switchport(self, port):
        json_cmd = {
            "commands":
            [
                "show interface ethernet " + port + " switchport"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp

    def write_memory(self): #requires running in enable mode
        json_cmd = {
            "commands":
            [
                "write memory"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp

    def configuration_write(self): #requires running in config mode
        json_cmd = {
            "commands":
            [
                "configuration write"
            ]
        }
        resp = self.send_cmd(self, json_cmd)
        return resp


    '''
    Combo Commands
    '''
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

    def config_access_mode_and_assign_pvid(self, vlan_id, port):
        '''
        Onyx Manual 5.5.1
        Configuring Access Mode and Assigning Port VLAN ID(PVID)

        The cmd sequence is:
        1.create a vlan
        2.change back to config mode
        3.enter the interface configuration mode
        4.from within the interface context, configure the interface mode to Access
        5.from within the interface context, configure the Access vlan membership
        6.change back to config mode
        '''
        json_cmd = {
            "commands":
            [
                "vlan " + str(vlan_id),
                "exit",
                "interface ethernet " + str(port),
                "switchport mode access",
                "switchport access vlan " + str(vlan_id),
                "exit"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp

    def config_hybrid_mode_and_assign_pvid(self, vlan_id, port):
        '''
        Onyx Manual 5.5.2
        Configuring Hybrid Mode and Assigning Port VLAN ID(PVID)

        The cmd sequence is:
        1.create a vlan
        2.change back to config mode
        3.enter the interface configuration mode
        4.from within the interface context, configure the interface mode to Hybrid
        5.From within the interface context, configure the Access vlan membership
        6.change back to config mode
        '''
        json_cmd = {
            "commands":
            [
                "vlan " + str(vlan_id),
                "exit",
                "interface ethernet "+ port,
                "switchport mode hybrid",
                "switchport access vlan " + str(vlan_id),
                "exit"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp


    def config_trunk_mode_vlan_membership(self, vlan_id, port):
        '''
        Onyx Manual 5.5.3
        Configuring Trunk Mode VLAN Membership

        The cmd sequence is:
        1.create a vlan
        2.change back to config mode
        3.enter the interface configuration mode
        4.from within the interface context, configure the interface mode to Trunk
        5.From within the interface context, configure the Access vlan membershipdd
        '''
        json_cmd = {
            "commands":
            [
                "vlan " + str(vlan_id),
                "exit",
                "interface ethernet "+ port,
                "switchport mode trunk",
                "exit"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp


    def config_hybrid_mode_vlan_membership(self, vlan_id, port):
        '''
        Onyx Manual 5.5.4
        Configuring Hybrid Mode and Assigning Port VLAN ID(PVID)

        The cmd sequence is:
        1.create a vlan
        2.change back to config mode
        3.enter the interface configuration mode
        4.from within the interface context, configure the interface mode to Hybrid
        5.From within the interface context, configure the allowed vlan membership
        '''
        json_cmd = {
            "commands":
            [
                "vlan " + str(vlan_id),
                "exit",
                "interface ethernet "+ port,
                "switchport mode hybrid",
                "switchport hybrid allowed-vlan add " + str(vlan_id),
                "exit"
            ]
        }
        resp = self.send_cmd(json_cmd)
        return resp


def client(sw_type, ip_address, log=None, db=None, mq=None):
    """Return an instance of SwitchMellanoxClient."""
    return SwitchMellanoxClient(sw_type, ip_address, log, db, mq)


