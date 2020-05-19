import importlib
from functools import wraps
from common.system.controller import Controller

#Switch module to use
sw = None

class SwitchController(Controller):
    def __new__(self, sw_type, ip_address, log=None, db=None, mq=None):
        #Get the switch type, load the required module
        global sw
        if sw_type == 'mellanox':
            sw = importlib.import_module('systems.switch.switch_mellanox.switch_mellanox_client')
        else:
            raise Exception('Invalid switch type provided, {} is not valid'.format(sw_type))
        # Create an instance of the class
        return Controller.__new__(self)


    def __init__(self, sw_type, ip_address, log=None, db=None, mq=None):
        Controller.__init__(self)
        self.ip_address = ip_address
        self.log = log
        self.db = db
        self.mq = mq
        self.running = False
        self.client = sw.client(sw_type, ip_address, log, db, mq)


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


    def __del__(self):
        pass


    @log_error
    def start(self):
        self.log.detail('SwitchController Start: ')
        return self.client.start()

    @log_error
    def stop(self):
        self.log.detail('SwitchController Stop: ')
        return self.client.stop()



    @log_error
    def is_running(self):
        return self.running


    @log_error
    def connect(self):
        self.log.detail('SwitchController Connect: ')
        return self.client.connect()


    @log_error
    def send_cmd(self, json_data):
        self.log.detail('SwitchController Send Cmd: ')
        return self.client.send_cmd(json_data)


    '''
    Basic Commands
    '''
    @log_error
    def enter_config_cmd(self):
        self.log.detail('SwitchController enter_config_cmd:')
        return self.client.enter_config_cmd()

    @log_error
    def back_to_config_mode(self):
        self.log.detail('SwitchController back_to_config_mode:')
        return self.client.back_to_config_mode()

    @log_error
    def no_vlan(self, vlan_id):
        self.log.detail('SwitchController no_vlan: vlan_id=%d',
                        vlan_id)
        return self.client.no_vlan(vlan_id)

    @log_error
    def show_interface_vlan(self, vlan_id):
        self.log.detail('SwitchController show_interface_vlan: vlan_id=%d',
                        vlan_id)
        return self.client.show_interface_vlan(vlan_id)

    @log_error
    def show_interface_ethernet_switchport(self, port):
        self.log.detail('SwitchController show_interface_ethernet_switchport: port=%s',
                        port)
        return self.client.show_interface_ethernet_switchport(port)

    @log_error
    def write_memory(self): #requires running in enable mode
        self.log.detail('SwitchController write_memory:')
        return self.client.write_memory()

    @log_error
    def configuration_write(self): #requires running in config mode
        self.log.detail('SwitchController configuration_write:')
        return self.client.configuration_write()


    '''
    Combo Commands
    '''
    @log_error
    def assign_port_to_vlan(self, port, vlan_id):
        self.log.detail('SwitchController assign_port_to_vlan: port=%s, vlan_id=%d',
                        port, vlan_id)
        return self.client.assign_port_to_vlan(port, vlan_id)

    @log_error
    def add_mode_port_to_vlan(self, port, mode, vlan_id):
        self.log.detail('SwitchController add_mode_port_to_vlan: port=%s, mode=%s, vlan_id=%d',
                        port, mode, vlan_id)
        return self.client.add_mode_port_to_vlan(port, mode, vlan_id)

    @log_error
    def associate_ip_to_vlan(self, vlan_id, ip_address):
        self.log.detail('SwitchController associate_ip_to_vlan: vlan_id=%d, ip_address=%s',
                        vlan_id, ip_address)
        return self.client.associate_ip_to_vlan(vlan_id, ip_address)

    @log_error
    def remove_ip_from_vlan(self, vlan_id, ip_address):
        self.log.detail("SwitchController remove_ip_from_vlan: vlan_id=%d, ip_address=%s",
                        vlan_id, ip_address)
        return self.client.remove_ip_from_vlan(vlan_id, ip_address)

    @log_error
    def config_access_mode_and_assign_pvid(self, vlan_id, port):
        self.log.detail('SwitchController config_access_mode_and_assign_pvid: vlan_id=%d, port=%s',
                        vlan_id, port)
        return self.client.config_access_mode_and_assign_pvid(vlan_id, port)


    @log_error
    def config_hybrid_mode_and_assign_pvid(self, vlan_id, port):
        self.log.detail('SwitchController config_hybrid_mode_and_assign_pvid: vlan_id=%d, port=%s',
                        vlan_id, port)
        return self.client.config_hybrid_mode_and_assign_pvid(vlan_id, port)


    @log_error
    def config_trunk_mode_vlan_membership(self, vlan_id, port):
        self.log.detail('SwitchController config_trunk_mode_vlan_membership: vlan_id=%d, port=%s',
                        vlan_id, port)
        return self.client.config_trunk_mode_vlan_membership(vlan_id, port)


    @log_error
    def config_hybrid_mode_vlan_membership(self, vlan_id, port):
        self.log.detail('SwitchController config_hybrid_mode_vlan_membership: vlan_id=%d, port=%s',
                        vlan_id, port)
        return self.client.config_hybrid_mode_vlan_membership(vlan_id, port)


def client(sw_type, ip_address, log=None, db=None, mq=None):
    """Return an instance of SwitchController."""
    return SwitchController(sw_type, ip_address, log, db, mq)

