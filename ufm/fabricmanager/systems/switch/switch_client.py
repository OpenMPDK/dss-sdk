"""
Template class for switch clients
"""
# from systems.switch import switch_constants
# from systems.switch.switch_arg import SwitchArg


class SwitchClientTemplate(object):
    def __init__(self, swArg):
        self.swArg = swArg
        self.log = swArg.log

        self.log.info("SwitchClientTemplate ip = {}".format(self.swArg.sw_ip))
        self.log.info("Init {}".format(self.__class__.__name__))

    def show_version(self):
        print('ERROR: SwitchClientTemplate.show_version must be implemented!')
        pass

    def show_vlan(self):
        print('ERROR: SwitchClientTemplate.show_vlan must be implemented!')
        pass

    def show_port(self):
        print('ERROR: SwitchClientTemplate.show_port must be implemented!')
        pass

    def delete_vlan(self, vlan_id):
        print('ERROR: SwitchClientTemplate.delete_vlan must be implemented!')
        pass

    def create_vlan(self, vlan_id):
        print('ERROR: SwitchClientTemplate. create_vlan must be implemented!')
        pass

    def associate_ip_to_vlan(self, vlan_id, ip_address):
        print('ERROR: SwitchClientTemplate.associate_ip_to_vlan must be implemented!')
        pass

    def remove_ip_from_vlan(self, vlan_id, ip_address):
        print('ERROR: SwitchClientTemplate.remove_ip_from_vlan must be implemented!')
        pass

    def assign_port_to_vlan(self, port, vlan_id):
        print('ERROR: SwitchClientTemplate.assign_port_to_vlan must be implemented!')
        pass

    def add_mode_port_to_vlan(self, port, mode, vlan_id):
        print('ERROR: SwitchClientTemplate.add_mode_port_to_vlan must be implemented!')
        pass

    # Major function called by SwitchMonitor to populate switch, port, vlan info into db
    def poll_to_db(self):
        print('ERROR: SwitchClientTemplate.poll_to_db must be implemented!')
        pass
