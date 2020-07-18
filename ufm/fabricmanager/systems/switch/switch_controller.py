
import json
import threading
from ufm_thread import UfmThread
from common.ufmdb.redfish.ufmdb_util import ufmdb_util
from rest_api.redfish.redfish_error_response import RedfishErrorResponse
from systems.switch.redfish_server import Rfserver
from systems.switch.switch_mellanox.switch_mellanox_client import SwitchMellanoxClient


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

        self.rf = Rfserver(event=self.event,
                           port=self.swArg.port,
                           process=self.action_handler)
        self.rf.start()

        if self.swArg.sw_type.lower() == 'mellanox':
            self.client = SwitchMellanoxClient(self.swArg)
            self.uuid = self.client.get_uuid()
        else:
            raise Exception('Invalid switch type, {} is not valid'.format(self.swArg.sw_type))

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
        try:

            payload = json.loads(jsonMessage)
            cmd = payload["cmd"]
            print(payload)

            dispatcher = {
                'DeleteVLAN': self.handle_delete_vlan,
                'CreateVLAN': self.handle_create_vlan,
                'NameVLAN': self.handle_name_vlan,

                'SetAccessPortVLAN': self.handle_set_access_port_vlan,
                'UnassignAccessPortVLAN': self.handle_unassign_access_port_vlan,
                'SetTrunkPortVLANsAll': self.handle_set_trunk_port_vlans_all,
                'SetTrunkPortVLANsRange': self.handle_set_trunk_port_vlans_range,
                'SetHybridPortAccessVLAN': self.handle_set_hybrid_port_access_vlan,
                'SetHybridPortAllowedVLAN': self.handle_set_hybrid_port_allowed_vlan,
                'RemoveHybridPortAllowedVLAN': self.handle_remove_hybrid_port_allowed_vlan,

                'EnablePortPfc': self.handle_enable_port_pfc,
                'DisablePortPfc': self.handle_disable_port_pfc,
                'EnableEcnMarking': self.handle_enable_ecn_marking,
                'DisableEcnMarking': self.handle_disable_ecn_marking,
                'ShowCongestionControl': self.handle_show_port_congestion_control,
                'ShowPfcCounters': self.handle_show_port_pfc_counters,

                'EnablePfcGlobally': self.handle_enable_pfc_globally,
                'DisablePfcGlobally': self.handle_disable_pfc_globally,
                'EnablePfcPerPriority': self.handle_enable_pfc_per_priority,
                'DisablePfcPerPriority': self.handle_disable_pfc_per_priority,

                'AnyCmd': self.handle_any_cmd
            }

            resp = {}
            method = dispatcher.get(cmd, lambda: "Invalid cmd")
            resp = method(payload)

            print(resp)
        except Exception as e:
            #print('Caught exc {e} in SwitchController.action_handler()')
            resp = RedfishErrorResponse.get_server_error_response(e)

        return resp

    '''
    vlan operations
    '''
    def handle_create_vlan(self, payload):
        vlan_id = payload['VLANId']

        resp = self.client.create_vlan(vlan_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'vlan ' + str(vlan_id):
                if item['status'] == 'OK':
                    self.client.poll_to_db()

        return json_obj

    def handle_name_vlan(self, payload):
        vlan_id = payload['VLANId']
        vlan_name = payload['Name']

        resp = self.client.name_vlan(vlan_id, vlan_name)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'vlan ' + str(vlan_id) + ' name ' + vlan_name:
                if item['status'] == 'OK':
                    pre = '/switches/' + self.uuid + '/VLANs/' + str(vlan_id) + '/name/'
                    self.db.delete_prefix(pre)
                    self.client.poll_to_db()

        return json_obj

    def handle_delete_vlan(self, payload):
        vlan_id = payload['VLANId']

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
    def handle_set_access_port_vlan(self, payload):
        port_id = payload['port_id']
        vlan_id = payload['VLANId']

        resp = self.client.set_access_port_vlan(port_id, vlan_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) + \
                                           ' switchport access vlan ' + str(vlan_id):
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_unassign_access_port_vlan(self, payload):
        port_id = payload['port_id']

        resp = self.client.unassign_access_port_vlan(port_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) +\
                                           ' no switchport access vlan':
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_set_trunk_port_vlans_all(self, payload):
        port_id = payload['port_id']

        resp = self.client.set_trunk_port_vlans_all(port_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) +\
                                           ' switchport trunk allowed-vlan all':
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_set_trunk_port_vlans_range(self, payload):
        port_id = payload['port_id']
        start_vlan_id = payload['RangeFromVLANId']
        end_vlan_id = payload['RangeToVLANId']

        resp = self.client.set_trunk_port_vlans_range(port_id, start_vlan_id, end_vlan_id)
        json_obj = resp.json()
        print(json_obj)

        # update db accordingly
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/trunk
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) +\
                                    ' switchport trunk allowed-vlan ' + str(start_vlan_id)+'-' + str(end_vlan_id):
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_set_hybrid_port_access_vlan(self, payload):
        port_id = payload['port_id']
        vlan_id = payload['VLANId']

        resp = self.client.set_hybrid_port_access_vlan(port_id, vlan_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) + \
                                           ' switchport access vlan ' + str(vlan_id):
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_set_hybrid_port_allowed_vlan(self, payload):
        port_id = payload['port_id']
        vlan_id = payload['VLANId']

        resp = self.client.set_hybrid_port_allowed_vlan(port_id, vlan_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) + \
                    ' switchport hybrid allowed-vlan add ' + str(vlan_id):
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_remove_hybrid_port_allowed_vlan(self, payload):
        port_id = payload['port_id']
        vlan_id = payload['VLANId']

        resp = self.client.remove_hybrid_port_allowed_vlan(port_id, vlan_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) + \
                                           ' switchport hybrid allowed-vlan remove ' + str(vlan_id):
                if item['status'] == 'OK':
                    self.remove_entry_for_port_mode_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_enable_pfc_globally(self, payload):
        resp = self.client.enable_pfc_globally()
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'dcb priority-flow-control enable force':
                if item['status'] == 'OK':
                    #self.remove_entry_for_pfc_change()
                    self.remove_all_per_switch()
                    self.client.poll_to_db()

        return json_obj

    def handle_disable_pfc_globally(self, payload):
        resp = self.client.disable_pfc_globally()
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'no dcb priority-flow-control enable force':
                if item['status'] == 'OK':
                    self.remove_entry_for_pfc_change()
                    self.client.poll_to_db()

        return json_obj

    def handle_enable_pfc_per_priority(self, payload):
        prio = payload['Priority']
        resp = self.client.enable_pfc_per_priority(prio)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'dcb priority-flow-control priority ' + str(prio) + ' enable':
                if item['status'] == 'OK':
                    self.remove_entry_for_pfc_change()
                    self.client.poll_to_db()

        return json_obj

    def handle_disable_pfc_per_priority(self, payload):
        prio = payload['Priority']
        resp = self.client.disable_pfc_per_priority(prio)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'no dcb priority-flow-control priority ' + str(prio) + ' enable':
                if item['status'] == 'OK':
                    self.remove_entry_for_pfc_change()
                    self.client.poll_to_db()

        return json_obj

    def handle_enable_port_pfc(self, payload):
        port_id = payload['port_id']
        resp = self.client.enable_port_pfc(port_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) + \
                                           ' dcb priority-flow-control mode on force':
                if item['status'] == 'OK':
                    self.remove_entry_for_pfc_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_disable_port_pfc(self, payload):
        port_id = payload['port_id']
        resp = self.client.disable_port_pfc(port_id)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == 'interface ethernet 1/' + str(port_id) + \
                                           ' no dcb priority-flow-control mode force':
                if item['status'] == 'OK':
                    self.remove_entry_for_pfc_change(port_id)
                    self.client.poll_to_db()

        return json_obj

    def handle_show_port_pfc_counters(self, payload):
        port_id = payload['port_id']
        prio = payload['Priority']

        resp = self.client.show_port_pfc_counters(port_id, prio)
        return resp.json()

    def handle_show_port_congestion_control(self, payload):
        port_id = payload['port_id']

        resp = self.client.show_port_congestion_control(port_id)
        return resp.json()

    def handle_enable_ecn_marking(self, payload):
        port_id = payload['port_id']
        tc = payload['TrafficClass']
        min_ab = payload['MinAbsoluteInKBs']
        max_ab = payload['MaxAbsoluteInKBs']

        resp = self.client.enable_ecn_marking_for_traffic_class_queue(port_id,
                                                                      tc,
                                                                      min_ab,
                                                                      max_ab)
        return resp.json()

    def handle_disable_ecn_marking(self, payload):
        port_id = payload['port_id']
        tc = payload['TrafficClass']

        resp = self.client.disable_ecn_marking_for_traffic_class_queue(port_id,
                                                                       tc)
        return resp.json()

    def handle_any_cmd(self, payload):
        any_cmd_str = payload['AnyCmdStr']

        resp = self.client.any_cmd(any_cmd_str)
        json_obj = resp.json()

        # update db accordingly
        for item in json_obj['results']:
            if item['executed_command'] == any_cmd_str:
                if item['status'] == 'OK':
                    self.remove_all_per_switch()
                    self.client.poll_to_db()

        return resp.json()

    def remove_entry_for_port_mode_change(self, port_id):
        # update db accordingly
        #
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/mode/access
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/52/network/access_vlan/1
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/50/mode/trunk
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/50/network/allowed_vlans/1
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/50/network/allowed_vlans/2
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/50/network/allowed_vlans/3
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/50/network/allowed_vlans/4
        pre = '/switches/' + self.uuid + '/ports/'

        kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/access_vlan')
        for k in kv_dict:
            vlan = k.split('/')[-1]
            self.db.delete_prefix('/switches/' + self.uuid + '/VLANs/' + str(vlan) +
                                  '/network/ports/' + str(port_id))

        kv_dict = ufmdb_util.query_prefix(pre + str(port_id) + '/network/allowed_vlans')
        for k in kv_dict:
            vlan = k.split('/')[-1]
            self.db.delete_prefix('/switches/' + self.uuid + '/VLANs/' + str(vlan) +
                                  '/network/ports/' + str(port_id))

        self.db.delete_prefix(pre + str(port_id) + '/mode/')
        self.db.delete_prefix(pre + str(port_id) + '/network/access_vlan/')
        self.db.delete_prefix(pre + str(port_id) + '/network/allowed_vlans/')

    def remove_entry_for_pfc_change(self, port_id=None):
        # update db accordingly
        #
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/pfc_status/enabled
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_enabled_list/3
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_enabled_list/4
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/0
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/1
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/2
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/5
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/6
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/switch_attributes/pfc/prio_disabled_list/7
        #
        # /switches/f1ec15f8-c832-11e9-8000-b8599f784980/ports/8/pfc/disabled

        if port_id:
            kv_dict = self.db.delete_prefix('/switches/' + self.uuid + '/ports/' + str(port_id) + '/pfc')
        else:
            kv_dict = self.db.delete_prefix('/switches/' + self.uuid + '/switch_attributes/pfc')

    def remove_all_per_switch(self):
        # remove everything starting with '/switches/' for this switch
        kv_dict = self.db.delete_prefix('/switches/' + self.uuid)


