
from ufm_thread import UfmThread
from systems.switch.switch_mellanox.switch_mellanox_client import SwitchMellanoxClient


class SwitchController(UfmThread):
    def __init__(self, swArg=None):
        self.swArg = swArg
        self.log = self.swArg.log
        self._running = False
        self.client = None
        self.uuid = None

        super(SwitchController, self).__init__()
        self.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        if self._running:
            self.stop()
        self.log.info("Del {}".format(self.__class__.__name__))


    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))

        if self.swArg.sw_type.lower() == 'mellanox':
            self.client = SwitchMellanoxClient(self.swArg)
            self.uuid = self.client.get_uuid()
        else:
            raise Exception('Invalid switch type provided, {} is not valid'.format(swArg.sw_type))

        self._running = True


    def stop(self):
        super(SwitchController, self).stop()
        self._running = False
        self.log.info("Stop {}".format(self.__class__.__name__))


    def is_running(self):
        return self._running

<<<<<<< HEAD
    '''
    switch operations
    '''
    def handle_show_version(self, vlan_id):
        resp = self.client.show_version(vlan_id)

    '''
    vlan operations
    '''
    def handle_show_vlan(self, vlan_id):
        resp = self.client.show_vlan(vlan_id)

    def handle_create_vlan(self, vlan_id):
        resp = self.client.create_vlan(vlan_id)
        #todo: adjust db entries

    def handle_delete_vlan(self, vlan_id):
        resp = self.client.delete_vlan(vlan_id)
        #todo: adjust db entries

    def handle_associate_ip_to_vlan(self, vlan_id, ip_address):
        resp = self.client.associate_ip_to_vlan(vlan_id, ip_address)
        #todo: adjust db entries

    def handle_remove_ip_from_vlan(self, vlan_id, ip_address):
        resp = self.client.remove_ip_from_vlan(vlan_id, ip_address)
        #todo: adjust db entries


    '''
    port operations
    '''
    def handle_show_port(self, port_id):
        resp = self.client.show_port(port_id)


    '''
    vlan and port operations
    '''
    def handle_assign_port_to_vlan(self, port_id, vlan_id):
        resp = self.client.assign_port_to_vlan(port_id, vlan_id)
        #todo: adjust db entries

=======

    '''
    switch operations
    '''
    def handle_show_version(self, vlan_id):
        resp = self.client.show_version(vlan_id)

    '''
    vlan operations
    '''
    def handle_show_vlan(self, vlan_id):
        resp = self.client.show_vlan(vlan_id)

    def handle_create_vlan(self, vlan_id):
        resp = self.client.create_vlan(vlan_id)
        #todo: adjust db entries

    def handle_delete_vlan(self, vlan_id):
        resp = self.client.delete_vlan(vlan_id)
        #todo: adjust db entries

    def handle_associate_ip_to_vlan(self, vlan_id, ip_address):
        resp = self.client.associate_ip_to_vlan(vlan_id, ip_address)
        #todo: adjust db entries

    def handle_remove_ip_from_vlan(self, vlan_id, ip_address):
        resp = self.client.remove_ip_from_vlan(vlan_id, ip_address)
        #todo: adjust db entries


    '''
    port operations
    '''
    def handle_show_port(self, port_id):
        resp = self.client.show_port(port_id)


    '''
    vlan and port operations
    '''
    def handle_assign_port_to_vlan(self, port_id, vlan_id):
        resp = self.client.assign_port_to_vlan(port_id, vlan_id)
        #todo: adjust db entries

    def handle_add_mode_port_to_vlan(self, port, mode, vlan_id):
        resp = self.client.add_mode_port_to_vlan(port, mode, vlan_id)
        #todo: adjust db entries
