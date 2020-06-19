
import time
import zmq
import json
import threading

from ufm_thread import UfmThread
from systems.switch.switch_mellanox.switch_mellanox_client import SwitchMellanoxClient


class Rfserver(threading.Thread):
    def __init__(self, event=None, port=None, process=None):
        self.event = event
        self.port = port
        self.process = process
        self.context = zmq.Context()
        super(Rfserver, self).__init__()


    def __del__(self):
        self.event.set()
        self.context.destroy()


    def run(self):
        socket = self.context.socket(zmq.REP)
        socket.bind("tcp://*:{}".format(self.port))

        self.event.clear()
        while not self.event.is_set():
            try:
                jsonRequest = socket.recv_json(flags=zmq.NOBLOCK)

                jsonResponse = self.process(jsonRequest)

                socket.send_json(jsonResponse)
            except KeyboardInterrupt:
                break
            except:
                time.sleep(1)
                continue

        socket.close()


    def stop(self):
        self.event.set()


class SwitchController(UfmThread):
    def __init__(self, swArg=None):
        self.swArg = swArg
        self.log = self.swArg.log
        self._running = False
        self.client = None
        self.uuid = None

        self.event = threading.Event()
        self.rf = None

        super(SwitchController, self).__init__()
        self.log.info("Init {}".format(self.__class__.__name__))


    def __del__(self):
        if self._running:
            self.stop()
        self.log.info("Del {}".format(self.__class__.__name__))


    def start(self):
        self.log.info("Start {}".format(self.__class__.__name__))

        self.rf = Rfserver(event=self.event, port=5501, process=self.getDataFromDB)
        self.rf.start()

        if self.swArg.sw_type.lower() == 'mellanox':
            self.client = SwitchMellanoxClient(self.swArg)
            self.uuid = self.client.get_uuid()
        else:
            raise Exception('Invalid switch type provided, {} is not valid'.format(swArg.sw_type))

        self._running = True
        super(SwitchController, self).start(threadName='SwitchController')


    def stop(self):
        self.rf.stop()
        self.rf.join()

        super(SwitchController, self).stop()
        self._running = False
        self.log.info("Stop {}".format(self.__class__.__name__))

    def is_running(self):
        return self._running


    def getDataFromDB(self, jsonMessage):
        responce = dict()
        responce['status'] = True
        responce['data'] = 42

        return responce


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
