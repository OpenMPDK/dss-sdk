"""
Template class for switch controller clients
"""

from common.system.controller import Controller

class SwitchControllerClient(Controller):
    def __init__(self, sw_type, ip_address, log=None, db=None, mq=None):
        self.running = False
        self.client = None
        print("ERROR: SwitchControllerClient.__init__ must be implemented!")

    def __del__(self):
        pass

    def start(self):
        print('ERROR: SwitchController.start must be implemented!')
        return

    def stop(self):
        print('ERROR: SwitchControllerClient.stop must be implemented!')
        return

    def is_running(self):
        return self.running

    def connect(self):
        print('ERROR: SwitchControllerClient.connect must be implemented!')
        return None

    def send_cmd(self, json_data):
        print('ERROR: SwitchControllerClient.send_cmd must be implemented!')
        return None


    '''
    Basic Commands
    '''
    def enter_config_cmd(self):
        print('ERROR: SwitchControllerClient.enter_config_cmd must be implemented!')
        return None

    def back_to_config_mode(self):
        print('ERROR: SwitchControllerClient.back_to_config_mode must be implemented!')
        return None

    def no_vlan(self, vlan_id):
        print('ERROR: SwitchControllerClient.no_vlan must be implemented!')
        return None

    def show_interface_vlan(self, vlan_id):
        print('ERROR: SwitchControllerClient.show_interface_vlan must be implemented!')
        return None

    def show_interface_ethernet_switchport(self, port):
        print('ERROR: SwitchControllerClient.show_interface_ethernet_switchport must be implemented!')
        return None

    def write_memory(self):
        print('ERROR: SwitchControllerClient.write_memory must be implemented!')
        return None

    def configuration_write(self):
        print('ERROR: SwitchControllerClient.configuration_write must be implemented!')
        return None


    '''
    Combo Commands
    '''
    def assign_port_to_vlan(self, port, vlan_id):
        print('ERROR: SwitchControllerClient.assign_port_to_vlan must be implemented!')
        return None

    def add_mode_port_to_vlan(self, port, mode, vlan_id):
        print('ERROR: SwitchControllerClient.add_mode_port_to_vlan must be implemented!')
        return None

    def associate_ip_to_vlan(self, vlan_id, ip_address):
        print('ERROR: SwitchControllerClient.associate_ip_to_vlan must be implemented!')
        return None

    def remove_ip_from_vlan(self, vlan_id, ip_address):
        print('ERROR: SwitchControllerClient.remove_ip_from_vlan must be implemented!')
        return None

    def config_access_mode_and_assign_pvid(self, vlan_id, port):
        print('ERROR: SwitchControllerClient.config_access_mode_and_assign_pvid must be implemented!')
        return None

    def config_hybrid_mode_and_assign_pvid(self, vlan_id, port):
        print('ERROR: SwitchControllerClient.config_hybrid_mode_and_assign_pvid must be implemented!')
        return None

    def config_trunk_mode_vlan_membership(self, vlan_id, port):
        print('ERROR: SwitchControllerClient.config_trunk_mode_vlan_membership must be implemented!')
        return None

    def config_hybrid_mode_vlan_membership(self, vlan_id, port):
        print("ERROR: SwitchControllerClient.config_hybrid_mode_vlan_membership must be implemented!")
        return None


def client(sw_type, ip_address, log=None, db=None, mq=None):
    return SwitchControllerClient(sw_type, ip_address, log, db, mq)

def log_errors(func):
    def func_wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            print(e)
            return None
    return func_wrapper

