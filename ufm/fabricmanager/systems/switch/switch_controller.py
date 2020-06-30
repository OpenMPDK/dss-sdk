
import time
import zmq
import json
import threading

from ufm_thread import UfmThread
from common.ufmdb.redfish.ufmdb_util import ufmdb_util
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
        self.db = self.swArg.db
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

        self.rf = Rfserver(event=self.event, port=self.swArg.port, process=self.action_handler)
        self.rf.start()

        if self.swArg.sw_type.lower() == 'mellanox':
            self.client = SwitchMellanoxClient(self.swArg)
            self.uuid = self.client.get_uuid()
        else:
            raise Exception('Invalid switch type provided, {} is not valid'.format(self.swArg.sw_type))

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

    def action_handler(self, jsonMessage):
        payload = json.loads(jsonMessage)

        resp = {}

        try:
            cmd = payload["cmd"]
            print('action_handler to process cmd : ' + cmd)
            if cmd == 'DeleteVLAN':
                resp = self.handle_delete_vlan(payload['vlan_id'])

            elif cmd == 'CreateVLAN':
                resp = self.handle_create_vlan(payload['vlan_id'])

            elif cmd == 'SetAccessPortVLAN':
                resp = self.handle_set_access_port_vlan(payload['port_id'],
                                                        payload['vlan_id'])
            elif cmd == 'UnassignAccessPortVLAN':
                resp = self.handle_unassign_access_port_vlan(payload['port_id'])

            elif cmd == 'SetTrunkPortVLANsAll':
                resp = self.handle_set_trunk_port_vlans_all(payload['port_id'])

            elif cmd == 'SetTrunkPortVLANsRange':
                resp = self.handle_set_trunk_port_vlans_range(payload['port_id'],
                                                              payload['start_vlan_id'],
                                                              payload['end_vlan_id'])
            else:
                resp = {"Status": "Not recognized cmd"}

            print(resp)
        except Exception as e:
            print(e)
            resp = {"Status": "failed"}

        return resp

    '''
    vlan operations
    '''
    def handle_create_vlan(self, vlan_id):
        resp = self.client.create_vlan(vlan_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'vlan ' + str(vlan_id):
                if item['status'] == 'OK':
                    self.client.poll_to_db()

        return json_obj

    def handle_delete_vlan(self, vlan_id):
        resp = self.client.delete_vlan(vlan_id)
        json_obj = resp.json()

        # update db accordingly
        # If a vlan is deleted successfully, there must be no port associated;
        # otherwise, it would fail.
        for item in json_obj['results']:
            if item['executed_command'] == 'no vlan ' + str(vlan_id):
                if item['status'] == 'OK':
                    pre = '/switches/' + self.uuid + '/VLANs/'
                    self.db.delete_prefix(pre + vlan_id)
                    self.db.delete(pre + 'list/' + vlan_id)
                    self.client.poll_to_db()
        return json_obj

    '''
    port operations
    '''
    def handle_set_access_port_vlan(self, port_id, vlan_id):
        resp = self.client.set_access_port_vlan(port_id, vlan_id)
        json_obj = resp.json()

        # update db accordingly
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/access
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/access_vlan/1
        for item in json_obj['results']:
            if item['executed_command'] == 'switchport access vlan ' + str(vlan_id):
                if item['status'] == 'OK':
                    pre = '/switches/' + self.uuid + '/ports/'

                    kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/access_vlan')
                    for k in kv_dict:
                        vlan = k.split('/')[-1]
                        self.db.delete_prefix('/switches/' + self.uuid + '/VLANs/' + str(vlan) + '/network/ports/' + str(port_id))

                    kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/allowed_vlans')
                    for k in kv_dict:
                        vlan = k.split('/')[-1]
                        self.db.delete_prefix('/switches/' + self.uuid + '/VLANs/' + str(vlan) + '/network/ports/' + str(port_id))

                    self.db.delete_prefix(pre + str(port_id) + '/mode')
                    self.db.delete_prefix(pre + str(port_id) + '/network/access_vlan')
                    self.db.delete_prefix(pre + str(port_id) + '/network/allowed_vlans')

                    self.client.poll_to_db()
        return json_obj

    def handle_unassign_access_port_vlan(self, port_id):
        resp = self.client.unassign_access_port_vlan(port_id)
        json_obj = resp.json()

        # update db accordingly
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/access
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/access_vlan/1
        for item in json_obj['results']:
            if item['executed_command'] == 'no switchport access vlan':
                if item['status'] == 'OK':
                    pre = '/switches/' + self.uuid + '/ports/' + str(port_id) + '/network/access_vlan/'

                    self.db.delete_prefix(pre)
                    self.client.poll_to_db()
        return json_obj

    def handle_set_trunk_port_vlans_all(self, port_id):
        resp = self.client.set_trunk_port_vlans_all(port_id)
        json_obj = resp.json()

        # update db accordingly
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/trunk
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/allowed_vlans/1
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/allowed_vlans/2
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/allowed_vlans/3
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/allowed_vlans/4
        for item in json_obj['results']:
            if item['executed_command'] == 'switchport trunk allowed-vlan all':
                if item['status'] == 'OK':
                    pre = '/switches/' + self.uuid + '/ports/'

                    kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/access_vlan')
                    for k in kv_dict:
                        vlan = k.split('/')[-1]
                        self.db.delete_prefix('/switches/' + self.uuid + '/VLANs/' + str(vlan) + '/network/ports/' + str(port_id))

                    self.db.delete_prefix(pre + str(port_id) + '/mode')
                    self.db.delete_prefix(pre + str(port_id) + '/network/access_vlan')

                    self.client.poll_to_db()

        return json_obj

    def handle_set_trunk_port_vlans_range(self, port_id, start_vlan_id, end_vlan_id):
        resp = self.client.set_trunk_port_vlans_range(port_id, start_vlan_id, end_vlan_id)
        json_obj = resp.json()
        print(json_obj)

        # update db accordingly
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/trunk
        for item in json_obj['results']:
            if item['executed_command'] == 'switchport trunk allowed-vlan '+str(start_vlan_id)+'-'+str(end_vlan_id):
                if item['status'] == 'OK':
                    pre = '/switches/' + self.uuid + '/ports/'

                    kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/access_vlan/')
                    for k in kv_dict:
                        vlan = k.split('/')[-1]
                        self.db.delete('/switches/' + self.uuid + '/VLANs/' + str(vlan) + '/network/ports/' + str(port_id))

                    kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/allowed_vlans')
                    for k in kv_dict:
                        vlan = k.split('/')[-1]
                        self.db.delete_prefix('/switches/' + self.uuid + '/VLANs/' + str(vlan) + '/network/ports/' + str(port_id))

                    self.db.delete_prefix(pre + str(port_id) + '/mode')
                    self.db.delete_prefix(pre + str(port_id) + '/network/access_vlan')
                    self.db.delete_prefix(pre + str(port_id) + '/network/allowed_vlans')

                    self.client.poll_to_db()

        return json_obj

    def handle_associate_ip_to_vlan(self, vlan_id, ip_address):
        resp = self.client.associate_ip_to_vlan(vlan_id, ip_address)
        #todo: adjust db entries

    def handle_remove_ip_from_vlan(self, vlan_id, ip_address):
        resp = self.client.remove_ip_from_vlan(vlan_id, ip_address)
        #todo: adjust db entries


