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


"""API library interfaces for the cluster operations
"""

import ast
from functools import wraps
import hashlib
import os
# import shutil
from subprocess import PIPE, Popen
import tarfile
# import tempfile
import time

import lib_constants
from etcdlib import etcd_api
from events.events_def_en import events, event_type
from events.events import get_events_from_etcd_db
from lib_exceptions import EtcdConnectionFail
from logger import cm_logger


def create_db_connection(ip_address='127.0.0.1', module_name='cluster_api',
                         config_file=None, section=None, log=None, port=2379):
    if not log:
        try:
            log = cm_logger.CMLogger(module_name, config_file, section).create_logger()
        except Exception as e:
            print("Error in creating the logger module for module %s with exception %s" % (module_name, e))
            log = None

    try:
        db_handle = etcd_api.EtcdAPI(ip_address, port=port, logger=log)
    except Exception as e:
        print("Error in creating the connection to ETCD server with exception %s" % e)
        raise e
    return db_handle


def __convert_network_speed_to_enum_val(speed_in_mb):
    speed_in_gb = int(speed_in_mb) / 1000
    ret = -1
    if speed_in_gb in lib_constants.NET_SPEED_DICT.keys():
        ret = lib_constants.NET_SPEED_DICT[speed_in_gb]
    '''
    if speed_in_gb == 1:
        ret = lib_constants.LS_1GBIT
    elif speed_in_gb == 10:
        ret = lib_constants.LS_10GBIT
    elif speed_in_gb == 50:
        ret = lib_constants.LS_50GBIT
    elif speed_in_gb == 100:
        ret = lib_constants.LS_100GBIT
    '''
    return ret


def do_pre_checks(func):
    """A decorator function which checks whether the ETCD DB connection is valid or not.
    :param func: function to run after doing the validation
    :return: returns wrapper function
    """
    @wraps(func)
    def do_check_init(db_handle, *args, **kwargs):
        if db_handle is None:
            try:
                db_handle = create_db_connection()
            except:
                raise EtcdConnectionFail("Etcd connection is not present")
        elif db_handle.client is None:
            try:
                db_handle.initialize()
            except Exception as excp:
                db_handle.logger and db_handle.logger.error('ETCD initialize raised the exception %s' % excp)
                raise excp
        return func(db_handle, *args, **kwargs)
    return do_check_init


@do_pre_checks
def __get_cluster_info(db_handle):
    """Returns all the management cluster related information
    The cluster dictionary will consist of the following fields
    cluster_id = ID of the CM cluster
    cluster_name = Name of the CM cluster
    cluster_creation_time = Date and time the CM cluster created
    cluster_communication_address = Public IP Address of the CM cluster
    Members information will be as follows
        cm_server_name = CM node name
        cm_server_address = CM Node IP Address
        cm_up_time_hours = CM Node uptime
        cm_instance_health = CM_UP/CM_DOWN
        cm_instance_mode = CM_ACTIVE/CM_PASSIVE
        cm_instance_store_stats = {
            bytes_total = DB size in bytes
            last_store_updated = DB last updated
    :return: Returns the cluster dictionary, number of node members, number of running node members, last_status_update
    :exception Returns an exception raised by the underlying DB
    """
    cluster_key_prefix = '/cluster/'
    try:
        cluster_out = db_handle.get_key_with_prefix(cluster_key_prefix)
        members = db_handle.get_members()
    except Exception as e:
        db_handle.logger and db_handle.logger.error('Error in getting the KVs from ETCD DB for prefix /cluster', exc_info=True)
        raise e
    members_total = 0
    members_up = 0
    last_status_update = 0
    cluster_info = {}
    if cluster_out:
        cluster_out = cluster_out['cluster']
        ctime = None
        try:
            ctime = db_handle.get_db_last_update_time(cluster_out['name'])
        except Exception as excp:
            db_handle.logger.error('Error in getting the db last update time with exception %s' % excp.message)

        if 'time_created' in cluster_out:
            cluster_info['cluster_creation_time'] = time.strftime('%a %d %b %Y %H:%M:%S GMT',
                                                                  time.gmtime(float(cluster_out['time_created'])))
        cluster_info['cluster_id'] = cluster_out.get('id', None)
        cluster_info['cluster_name'] = cluster_out.get('name', None)
        cluster_info['cluster_client_communication_address'] = cluster_out.get('ip_address', None)
        if cluster_info['cluster_client_communication_address']:
            cluster_info['cluster_client_communication_address'] += ":8080"
        cluster_info['members'] = []
        for member in members:
            cluster_member = dict()
            cluster_member['cm_server_name'] = member.name
            if member.name not in cluster_out:
                continue

            cluster_member['cm_id'] = cluster_out[member.name].get('id', None)
            cluster_member['cm_server_address'] = cluster_out[member.name].get('ip_address', None)
            if 'uptime' in cluster_out[member.name]:
                cluster_member['cm_up_time_hours'] = cluster_out[member.name]['uptime']
            if ('status' in cluster_out[member.name] and cluster_out[member.name]['status'] == 'up'):
                cluster_member['cm_instance_health'] = lib_constants.CM_UP
                members_up += 1
            else:
                cluster_member['cm_instance_health'] = lib_constants.CM_DOWN
            if 'leader' in cluster_out and cluster_out['leader'] == member.name:
                cluster_member['cm_instance_mode'] = lib_constants.CM_ACTIVE
            else:
                cluster_member['cm_instance_mode'] = lib_constants.CM_PASSIVE
            cluster_member['cm_instance_store_stats'] = dict()
            cluster_member['cm_instance_store_stats']['bytes_total'] = cluster_out[member.name].get('db_size', None)
            cluster_member['cm_server_kb_total'] = cluster_out[member.name].get('total_capacity_in_kb', None)
            cluster_member['cm_server_avail_percent'] = cluster_out[member.name].get('space_avail_percent', None)
            if ctime:
                cluster_member['cm_instance_store_stats']['last_store_updated'] = time.strftime(
                    '%a %d %b %Y %H:%M:%S GMT', time.gmtime(float(ctime)))
            if 'status_updated' in cluster_out[member.name]:
                if int(cluster_out[member.name]['status_updated']) > last_status_update:
                    last_status_update = int(cluster_out[member.name]['status_updated'])

            members_total += 1
            cluster_info['members'].append(cluster_member)

    return cluster_info, members_total, members_up, last_status_update


@do_pre_checks
def get_cluster_info(db_handle):
    """Returns all the management cluster related information.
    It is the caller responsibility to marshall the data to their needs
    The cluster dictionary will consist of the following fields
    cluster_id = ID of the CM cluster
    cluster_name = Name of the CM cluster
    cluster_creation_time = Date and time the CM cluster created
    cluster_communication_address = Public IP Address of the CM cluster
    Members information will be as follows
        cm_server_name = CM node name
        cm_server_address = CM Node IP Address
        cm_up_time_hours = CM Node uptime
        cm_instance_health = CM_UP/CM_DOWN
        cm_instance_mode = CM_ACTIVE/CM_PASSIVE
        cm_instance_store_stats = {
            bytes_total = DB size in bytes
            last_store_updated = DB last updated
    :return: Returns the cluster dictionary
    """
    try:
        cluster_info, _, _, _ = __get_cluster_info(db_handle)
    except Exception as e:
        raise e
    return cluster_info


@do_pre_checks
def get_events(db_handle):
    """Get all the events in the DB from the key prefix /events and convert
    to the appropriate output format
    :param db_handle: Instance of DB class
    :return: List of events with the structure
            event = {'cluster_event_type': 'CM_CONFIG_CHANGED',
                     'cluster_event_severity': 'WARNING',
                     'cluster_event_message': 'Too many leader changes in an hour',
                     'cluster_event_creation_time': 'Mon Apr  2 16:58:50 PDT 2018'
                     }
    :exception Raises an exception in case of DB exception
    """
    l_events = []
    try:
        el = get_events_from_etcd_db(db_handle)
    except Exception as e:
        raise e

    for e in el:
        event = dict()
        event_def = events[e['name']]
        event['cluster_event_severity'] = int(event_def['id'][2:4]) - 1
        event['cluster_event_type'] = event_type[event_def['cluster_change']]
        event_args = e['args']
        event['cluster_event_message'] = event_def['msg'].format(**event_args)
        event['cluster_event_creation_time'] = (
            time.strftime('%a %d %b %Y %H:%M:%S GMT',
                          time.gmtime(float(e['timestamp']))))
        event['timestamp'] = int(e['timestamp'])
        l_events.append(event)

    event_list = sorted(l_events,
                        key=lambda e: e['timestamp'],
                        reverse=True)
    return event_list


@do_pre_checks
def __get_targets_info(db_handle):
    """Gets the target information from the DB
    :param db_handle: Instance of DB class
    :return: A tuple of (targets, subsystems_total, subsystems_up, agents_total, agents_up)
    :exception Raises an exception in case of DB exception
    """
    targets = []
    subsystems_total = 0
    subsystems_up = 0
    agents_total = 0
    agents_up = 0
    last_status_updated = 0
    server_key_prefix = '/object_storage/servers/'
    try:
        json_out = db_handle.get_key_with_prefix(server_key_prefix)
    except Exception as e:
        raise e
    if json_out:
        servers_out = json_out['object_storage']['servers']
        if 'list' in servers_out:
            agent_hb_list = servers_out.pop('list')
        else:
            agent_hb_list = None
        target_list = servers_out.keys()
        curr_time = time.time()
        for target_id in target_list:
            agents_total += 1
            if agent_hb_list and target_id in agent_hb_list:
                if int(curr_time - float(agent_hb_list[target_id])) < lib_constants.MAX_TIME_LAPSE:
                    agents_up += 1
                if float(agent_hb_list[target_id]) > last_status_updated:
                    last_status_updated = float(agent_hb_list[target_id])
            target_entry = dict()
            target_entry['target_id'] = target_id
            target_entry['target_kb_total'] = 0
            target_entry['target_kb_avail'] = 0

            try:
                target_server_name = servers_out[target_id]['server_attributes']['identity']['Hostname']
                target_entry['target_server_name'] = target_server_name
            except KeyError:
                db_handle.logger.error("Error in looking for server name for target id %s" % target_id)
                target_entry['target_server_name'] = 'None'

            try:
                subsystems_json = servers_out[target_id]['kv_attributes']['config']['subsystems']
            except KeyError as excp:
                db_handle.logger.error('Could not get subsystem attributes %s', excp)
                subsystems_json = dict()

            try:
                drives_json = json_out['object_storage']['servers'][target_id]
                drives_json = drives_json['server_attributes']['storage']['nvme']['devices']
            except KeyError as excp:
                db_handle.logger.error('Could not get drive attributes %s', excp)
                drives_json = dict()

            try:
                network_json = json_out['object_storage']['servers'][target_id]['server_attributes']
                network_json = network_json['network']['interfaces']
            except KeyError as excp:
                db_handle.logger.error('Could not get network attributes %s', excp)
                network_json = dict()

            subsystems = []
            for nqn in subsystems_json:
                drive_numa_list = list()
                subsystem_entry = dict()
                subsystem_entry['subsystem_name'] = nqn
                subsystem_entry['target_server_name'] = target_entry.get('target_server_name', None)
                subsystem_entry['subsystem_nqn_id'] = subsystems_json[nqn].get('UUID', None)
                # TODO: Hardcoded the nqn_nsid to 1 till the code is developed
                subsystem_entry['subsystem_nqn_nsid'] = 1
                subsystem_entry['cmd_status'] = subsystems_json[nqn].get(
                    'cmd_status', 'None')
                if 'status' in subsystems_json[nqn]:
                    subsystem_entry['subsystem_status'] = lib_constants.SUBSYSTEM_OK
                    subsystems_up += 1
                else:
                    subsystem_entry['subsystem_status'] = lib_constants.SUBSYSTEM_DOWN

                namespaces_json = subsystems_json[nqn]['namespaces']
                drives = []
                subsystem_used_size = 0
                subsystem_total_size = 0
                for ns_key, ns_value in namespaces_json.iteritems():
                    drive_entry = dict()
                    drive_entry['drive_address'] = ns_value['PCIAddress']
                    serial = ns_value['Serial']
                    drive_entry['drive_serial_number'] = serial
                    drive_entry['drive_id'] = serial
                    drive_entry['drive_is_nvme'] = 1
                    drive_entry['drive_is_kv'] = 1
                    drive_json_data = drives_json.get(serial, None)
                    if drive_json_data:
                        drive_entry['drive_model'] = drive_json_data.get('Model', None)
                        drive_entry['drive_fw_version'] = drive_json_data.get('FirmwareRevision', None)
                        drive_entry['drive_size_mb'] = int(drive_json_data.get('SizeInBytes', 0)) / (1024 * 1024)
                        drive_entry['drive_critical_warning'] = int(drive_json_data.get('DriveCriticalWarning', 0))
                        drive_entry['drive_temperature_C'] = int(drive_json_data.get('DriveTemperature', 0))
                        drive_entry['drive_percentage_used_space'] = int(float(drive_json_data.get(
                            'DiskUtilizationPercentage', 0)))
                        drive_entry['drive_data_units_read'] = int(drive_json_data.get('DriveDataUnitsRead', 0))
                        drive_entry['drive_data_units_written'] = int(drive_json_data.get(
                            'DriveDataUnitsWritten', 0))
                        drive_entry['drive_host_read_commands'] = int(drive_json_data.get(
                            'DriveHostReadCommands', 0))
                        drive_entry['drive_host_write_commands'] = int(drive_json_data.get(
                            'DriveHostWriteCommands', 0))
                        drive_entry['drive_power_cycles'] = int(drive_json_data.get('DrivePowerCycles', 0))
                        drive_entry['drive_power_on_hours'] = int(drive_json_data.get('DrivePowerOnHours', 0))
                        drive_entry['drive_unsafe_shutdowns'] = int(drive_json_data.get(
                            'DriveUnsafeShutdowns', 0))
                        drive_entry['drive_media_errors'] = int(drive_json_data.get('DriveMediaErrors', 0))
                        drive_entry['drive_num_err_log_entries'] = int(drive_json_data.get(
                            'DriveNumErrLogEntries', 0))
                        subsystem_used_size += int(drive_json_data.get('DiskUtilizationInBytes', 0))
                        subsystem_total_size += int(drive_json_data.get('SizeInBytes', 0))

                        drive_numa_list.append(int(drive_json_data['NUMANode']))
                    drives.append(drive_entry)
                subsystem_entry['mapped_drives'] = drives
                subsystem_entry['subsystem_kb_used'] = int(subsystem_used_size / 1024)
                subsystem_entry['subsystem_kb_total'] = int(subsystem_total_size / 1024)
                subsystem_entry['subsystem_kb_avail'] = subsystem_entry['subsystem_kb_total'] - subsystem_entry['subsystem_kb_used']
                if subsystem_entry['subsystem_kb_total']:
                    subsystem_entry['subsystem_space_avail_percent'] = int(
                        subsystem_entry['subsystem_kb_avail'] * 100 / subsystem_entry['subsystem_kb_total'])
                drive_numa_list = list(set(drive_numa_list))
                drive_numa_list_len = len(drive_numa_list)

                transport_modes = []
                try:
                    transport_address_json = subsystems_json[nqn]['transport_addresses']
                except KeyError:
                    db_handle.logger.error("Could not get transport address attributes for NQN %s" % (subsystem_entry['subsystem_nqn_id']))
                    transport_address_json = dict()

                for macaddr, value in transport_address_json.iteritems():
                    transport_mode_entry = dict()
                    transport_mode_entry['subsystem_address'] = value['traddr']
                    transport_mode_entry['subsystem_port'] = value['trsvcid']
                    if value['trtype'].upper() == 'RDMA':
                        transport_mode_entry['subsystem_type'] = lib_constants.SUBSYSTEM_TYPE_RDMA
                    elif value['trtype'].upper() == 'TCP':
                        transport_mode_entry['subsystem_type'] = lib_constants.SUBSYSTEM_TYPE_TCP
                    else:
                        db_handle.logger.error("Unknown SUBSYSTEM_TYPE = {}".format(value['trtype']))
                    if value['adrfam'] == 'IPv4':
                        transport_mode_entry['subsystem_addr_fam'] = lib_constants.SUBSYSTEM_ADDR_FAMILY_IPV4
                    else:
                        transport_mode_entry['subsystem_addr_fam'] = lib_constants.SUBSYSTEM_ADDR_FAMILY_IPV6
                    transport_mode_entry['subsystem_interface_speed'] = \
                        __convert_network_speed_to_enum_val(network_json[macaddr]['Speed'])
                    if drive_numa_list_len == 0 or drive_numa_list_len > 1:
                        transport_mode_entry['subsystem_interface_numa_aligned'] = False
                    elif drive_numa_list[0] == int(network_json[macaddr]['NUMANode']):
                        transport_mode_entry['subsystem_interface_numa_aligned'] = True
                    else:
                        transport_mode_entry['subsystem_interface_numa_aligned'] = False
                    if 'Status' in network_json[macaddr] and network_json[macaddr]['Status'] == 'up':
                        transport_mode_entry['subsystem_interface_status'] = (
                            lib_constants.SUBSYSTEM_IFACE_STATUS_UP)
                    else:
                        transport_mode_entry['subsystem_interface_status'] = (
                            lib_constants.SUBSYSTEM_IFACE_STATUS_DOWN)
                    transport_modes.append(transport_mode_entry)
                subsystem_entry['subsystem_transport'] = transport_modes

                subsystems_total += 1
                subsystems.append(subsystem_entry)
                target_entry['target_kb_total'] += subsystem_entry['subsystem_kb_total']
                target_entry['target_kb_avail'] += subsystem_entry['subsystem_kb_avail']
            target_entry['subsystem_stats'] = subsystems
            target_entry['number_of_subsystems'] = len(subsystems)
            try:
                target_entry['target_up_time_hours'] = int(servers_out[target_id]['server_attributes']['uptime'])
            except KeyError as excp:
                db_handle.logger.error("Cannot get the target uptime %s", excp)

            targets.append(target_entry)

    return targets, subsystems_total, subsystems_up, agents_total, agents_up, int(last_status_updated)


@do_pre_checks
def get_cluster_health(db_handle, detail=False):
    """Get the cluster health
    :param db_handle: Instance of DB class
    :param detail: Default value is false.
                   If true, gets the detailed health information of cluster, targets, subsystems and disks
    :return:
    """
    cluster_info = dict()
    try:
        cm_info, cm_members_total, cm_members_up, cm_last_status = __get_cluster_info(db_handle)
        targets, subsystems_total, subsystems_up, agents_total, agents_up, tgt_last_status = __get_targets_info(db_handle)
    except Exception as e:
        raise e
    cluster_info['number_of_cm_server_instances'] = cm_members_total
    cluster_info['number_of_up_cm_server_instances'] = cm_members_up

    cluster_info['number_of_target_subsystems'] = subsystems_total
    cluster_info['number_of_up_target_subsystems'] = subsystems_up
    cluster_info['number_of_cm_agent_instances'] = agents_total
    cluster_info['number_of_up_cm_agent_instances'] = agents_up

    if cm_members_total != cm_members_up or subsystems_up != subsystems_total:
        if (cm_members_total - cm_members_up) > (cm_members_total - 1) / 2:
            cluster_info['overall_cluster_status'] = lib_constants.HEALTH_FAILED
        elif (cm_members_total - cm_members_up) == (cm_members_total - 1) / 2:
            cluster_info['overall_cluster_status'] = lib_constants.HEALTH_CRITICAL
        else:
            cluster_info['overall_cluster_status'] = lib_constants.HEALTH_WARNING
    else:
        cluster_info['overall_cluster_status'] = lib_constants.HEALTH_OK

    try:
        cluster_uptime_in_secs = db_handle.get_key_value('/cluster/uptime_in_seconds')
        if cluster_uptime_in_secs:
            cluster_info['cluster_up_time_hours'] = int(cluster_uptime_in_secs) / 3600
    except:
        db_handle.logger.error('Exception getting cluster_up_time_hours', exc_info=True)
        pass
    cluster_kb_total = 0
    cluster_kb_avail = 0
    for t in targets:
        cluster_kb_total = t['target_kb_total']
        cluster_kb_avail = t['target_kb_avail']
    if cluster_kb_total:
        cluster_info['space_avail_percent'] = int(cluster_kb_avail * 100 / cluster_kb_total)

    last_status_update = max(cm_last_status, tgt_last_status)
    cluster_info['last_status_update'] = time.strftime('%a %d %b %Y %H:%M:%S GMT',
                                                       time.gmtime(float(last_status_update)))
    if detail:
        cluster_info['health_services'] = dict()
        if 'members' in cm_info:
            cluster_info['health_services']['cms'] = cm_info['members']
        cluster_info['health_services']['targets'] = targets
        try:
            cluster_info['health_services']['events'] = get_events(db_handle)
        except:
            db_handle.logger.error('Exception getting events', exc_info=True)
            pass
    return cluster_info


@do_pre_checks
def get_cluster_version(db_handle):
    """Get the version of various components like CM node, etcd, target, subsystem, agent, disks in the cluster
    :param db_handle: Instance of DB class
    :return: Returns a dictionary of the cluster components and their version
    :exception Returns an exception raised by the DB
    """
    cluster_key_prefix = '/cluster/'
    try:
        cluster_out = db_handle.get_key_with_prefix(cluster_key_prefix)
        member_list = db_handle.get_members()
    except Exception as e:
        db_handle.logger.error('Exception in getting the KVs from ETCD DB with prefix /cluster', exc_info=True)
        raise e
    version_out = {}
    cm_version = []
    cm_agent_version = []

    for member in member_list:
        cm_version_entry = dict()
        cm_version_entry['cm_server_name'] = member.name
        if cluster_out:
            cm_version_entry['cm_id'] = cluster_out['cluster'][member.name]['id']
        cm_version_entry['version'] = lib_constants.CM_VERSION
        cm_version.append(cm_version_entry)

    version_out['cm_version'] = cm_version
    target_software_version = []
    server_key_prefix = '/object_storage/servers/'
    try:
        json_out = db_handle.get_key_with_prefix(server_key_prefix)
    except Exception as e:
        db_handle.logger.error('Exception in getting the KVs from ETCD DB with prefix /object_storage/servers', exc_info=True)
        raise e
    if json_out:
        targets = json_out['object_storage']['servers'].keys()
        if 'list' in targets:
            targets.remove('list')
        servers_out = json_out['object_storage']['servers']
        for target_id in targets:
            target_entry = dict()
            target_entry['target_id'] = target_id
            try:
                target_server_name = servers_out[target_id]['server_attributes']['identity']['Hostname']
                target_entry['target_server_name'] = target_server_name
            except KeyError:
                db_handle.logger.error("Error in looking for server name for target id %s" % target_id)
                target_entry['target_server_name'] = 'None'

            if 'kv_attributes' in servers_out[target_id]:
                subsystems_json = servers_out[target_id]['kv_attributes']['config']['subsystems']
            else:
                subsystems_json = dict()
            subsystems = []
            for nqn in subsystems_json:
                subsystem_entry = dict()
                subsystem_entry['subsystem_nqn'] = nqn
                subsystem_entry['subsystem_nqn_id'] = subsystems_json[nqn].get('UUID', None)
                transport_modes = []
                transport_address_json = subsystems_json[nqn]['transport_addresses']
                for key, value in transport_address_json.iteritems():
                    transport_mode_entry = dict()
                    transport_mode_entry['subsystem_address'] = value['traddr']
                    transport_mode_entry['subsystem_port'] = value['trsvcid']
                    transport_modes.append(transport_mode_entry)
                subsystem_entry['subsystem_transport'] = transport_modes

                namespaces_json = subsystems_json[nqn]['namespaces']
                drives = []
                drives_json = json_out['object_storage']['servers'][target_id]
                drives_json = drives_json['server_attributes']['storage']['nvme']['devices']
                for ns_key, ns_value in namespaces_json.iteritems():
                    drive_entry = dict()
                    drive_entry['drive_address'] = ns_value['PCIAddress']
                    serial = ns_value['Serial']
                    drive_entry['drive_serial_number'] = serial
                    drive_entry['drive_id'] = serial
                    drive_entry['drive_is_nvme'] = 1
                    drive_entry['drive_is_kv'] = 1
                    drive_entry['drive_model'] = drives_json[serial]['Model']
                    drive_entry['drive_fw_version'] = drives_json[serial]['FirmwareRevision']
                    drives.append(drive_entry)
                subsystem_entry['drives'] = drives
                subsystems.append(subsystem_entry)
            target_entry['subsystems'] = subsystems
            if 'target' in servers_out[target_id]:
                target_entry['version'] = servers_out[target_id]['target'].get('version', None)
                target_entry['version_hash'] = servers_out[target_id]['target'].get('hash', None)
            target_software_version.append(target_entry)

            agent_version_entry = dict()
            if 'agent' in servers_out[target_id]:
                target = servers_out[target_id]
                if 'server_attributes' in target:
                    agent_version_entry['target_server_name'] = target['server_attributes']['identity']['Hostname']
                agent_version_entry['target_id'] = target_id
                if 'agent' in target:
                    agent_version_entry['version'] = target['agent'].get('version', None)
            cm_agent_version.append(agent_version_entry)

    version_out['target_software_version'] = target_software_version
    version_out['cm_agent_version'] = cm_agent_version

    return version_out


@do_pre_checks
def get_cluster_map(db_handle, epoch=None):
    """Get the cluster map containing the targets, subsystems, and disk information
    :param db_handle: Instance of DB class
    :param epoch: The cluster map at the given timestamp
    :return: Returns cluster map
    :exception Returns exception raised by DB
    """
    if epoch:
        db_handle.logger.info("get_cluster_map called with epoch %s" % epoch)

    cluster_map = dict()
    try:
        cm_info, cm_members_total, cm_members_up, cm_last_status = __get_cluster_info(db_handle)
        targets, subsystems_total, subsystems_up, agents_total, agents_up, tgt_last_status = __get_targets_info(
            db_handle)
    except Exception as e:
        db_handle.logger.error('Exception in getting cluster info', exc_info=True)
        raise e

    cluster_map['cm_maps'] = cm_info['members']
    subsystem_maps = []

    if cm_members_total != cm_members_up or subsystems_up != subsystems_total:
        if (cm_members_total - cm_members_up) > (cm_members_total - 1) / 2:
            cluster_map['overall_cluster_status'] = lib_constants.HEALTH_FAILED
        else:
            cluster_map['overall_cluster_status'] = lib_constants.HEALTH_CRITICAL
    else:
        cluster_map['overall_cluster_status'] = lib_constants.HEALTH_OK

    last_status_update = max(cm_last_status, tgt_last_status)
    cluster_map['last_status_update'] = time.strftime(
        '%a %d %b %Y %H:%M:%S GMT', time.gmtime(float(last_status_update)))

    for target in targets:
        for subsystem in target['subsystem_stats']:
            if subsystem['cmd_status'] != 'success':
                continue
            subsystem_maps.append(subsystem)
    cluster_map['subsystem_maps'] = subsystem_maps
    return cluster_map


@do_pre_checks
def start_cm(db_handle):
    ret = 0
    for service in ['etcd', 'etcdmonitor']:
        cmd = 'systemctl show ' + service
        pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = pipe.communicate()
        if not err and "SubState=dead" in out:
            cmd = "systemctl start " + service
            pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
            out, err = pipe.communicate()
            if err:
                db_handle.logger.error("Error in starting the service %s,  %s"
                                       % (service, err))
                ret = -1
                break
        elif err:
            db_handle.logger.error("Error in getting the service info %s, %s"
                                   % (service, err))
            ret = -1
    if ret:
        db_handle.initialize()
    return ret


@do_pre_checks
def stop_cm(db_handle):
    ret = 0
    for service in ['etcdmonitor', 'etcd']:
        cmd = 'systemctl show ' + service
        pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = pipe.communicate()
        if not err and "SubState=running" in out:
            cmd = "systemctl stop " + service
            pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
            out, err = pipe.communicate()
            if err:
                db_handle.logger.error("Error in stopping the service %s,  %s"
                                       % (service, err))
                ret = -1
                break
        elif err:
            db_handle.logger.error("Error in getting the service info %s, %s"
                                   % (service, err))
            ret = -1
    if ret:
        db_handle.deinitialize()
    return ret


@do_pre_checks
def manage_service(db_handle, service_name, action, node_name):
    cmd = ('sshpass -p msl-ssg ssh root@' + node_name + ' systemctl ' + action + ' ' + service_name)
    db_handle.logger.info('manage service called for %s', cmd)
    cmd = cmd.split()
    pipe = Popen(cmd, shell=False, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    if action == 'stop':
        action_verb = 'stopp'
    else:
        action_verb = action
    if err:
        db_handle.logger.error('Error in %sing the service %s on node %s - %s',
                               action_verb, service_name, node_name, err)
        return -1
    db_handle.logger.info('Successfully %sed the %s on node %s - %s',
                          action_verb, service_name, node_name, out)
    return 0


@do_pre_checks
def get_nkv_config(db_handle, nkv_client):
    status = True
    nkv_key = '/nkv/' + nkv_client
    try:
        data = db_handle.get_key_value(nkv_key)
    except:
        db_handle.logger.exception('Error in getting the value for %s',
                                   nkv_key)
        data = {}
        status = False
    nkv_config = dict()
    nkv_config['client'] = nkv_client
    nkv_config['config'] = str(data)
    return nkv_config, status


@do_pre_checks
def save_nkv_config(db_handle, nkv_client, config):
    nkv_key = '/nkv/' + nkv_client
    ret = True
    try:
        db_handle.save_key_value(nkv_key, str(config))
    except:
        db_handle.logger.exception('Error in saving the value %s for %s',
                                   str(config), nkv_client)
        ret = False
    return ret


@do_pre_checks
def initiate_sw_upgrade(db_handle, upgrade_image_file, force_flag):
    """
    :param db_handle: ETCD DB handle
    :param upgrade_image_file: Image file to perform upgrade
    :param force_flag: Forcefully upgrade
    :return:
        0 - Success
        1 - Invalid input
        2 - Upgrade in progress
        3 - Upgrade failed
        4 - ETCD DB put/get failure
        5 - Checksum mismatch
        6 - Image copy failed
        7 - Compatibility check failed
        8 - Software up to date
    """

    etc_dir = '/etc/nkv-agent/'
    upgrade_image_dir = etc_dir + 'upgrade_dir/'
    image_basename = os.path.basename(upgrade_image_file)

    # Check if any upgrade going on and return error
    try:
        curr_image = db_handle.get_key_value('/software/current_image')
        if curr_image and curr_image == image_basename:
            db_handle.logger.info('Software image up to date')
            return 8
        img_in_progress = db_handle.get_key_value(
            '/software/upgrade/progress/image_name')
    except:
        db_handle.logger.exception('Exception in getting the key of '
                                   'inprogress image')
        return 4

    if img_in_progress:
        db_handle.logger.info('Upgrade already in progress with image %s. '
                              'Return invalid', img_in_progress)
        return 2

    # Check the image consistency
    try:
        tarobj = tarfile.open(upgrade_image_file)
        chksum_read = None
        file_checksum_value = None
        for elem in tarobj.getmembers():
            if elem.name.endswith('md5sum'):
                chksum_read = tarobj.extractfile(elem.name).read().split()[0]
            if elem.name.endswith('tar'):
                obj = tarobj.extractfile(elem.name)
                file_checksum_value = hashlib.md5(obj.read()).hexdigest()
    except:
        db_handle.logger.exception('Exception in untar')
        return 3

    if not chksum_read:
        db_handle.logger.error('Could not read the checksum from the file %s',
                               image_basename.split('.')[0] + '.md5sum')
        return 5

    if chksum_read != file_checksum_value:
        db_handle.logger.error('Checksum mismatch, Given %s, Found %s',
                               file_checksum_value, chksum_read)
        return 5

    # Copy the file to various nodes
    try:
        cluster_info, _, _, _ = __get_cluster_info(db_handle)
    except:
        db_handle.logger.exception('Error in getting cluster member details')
        return 4

    if 'members' not in cluster_info:
        db_handle.logger.error('Wrong with the cluster info %s',
                               str(cluster_info))
        return 4

    members = cluster_info['members']
    for member in members:
        ipaddr = member['cm_server_address']
        cmd = (
            'ssh root@' + ipaddr + ' mkdir -p ' + upgrade_image_dir + '&&'
            + 'scp ' + upgrade_image_file + ' root@' + ipaddr
            + ':' + upgrade_image_dir + image_basename)
        db_handle.logger.info('Cmd to copy file %s', cmd)
        p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = p.communicate()
        if err:
            db_handle.logger.error('Error in copying the file [%s] err %s',
                                   cmd, err)
            return 6
        if out:
            db_handle.logger.info('cmd %s, out %s', cmd, out)

    # Update the etcd with the new upgrade file
    try:
        db_handle.save_multiple_key_values(
            {
                '/software/upgrade/progress/image_dir': upgrade_image_dir,
                '/software/upgrade/progress/image_name': image_basename,
                '/software/upgrade/progress/checksum': file_checksum_value,
            }
        )
    except:
        db_handle.logger.exception('Exception in updating the upgrade image '
                                   'name to DB')
        return 4

    return 0


@do_pre_checks
def lib_get_sw_upgrade_progress(db_handle):
    upgrade_out = dict()
    try:
        db_dict = db_handle.get_key_with_prefix('/software')
    except:
        db_handle.logger.exception('Exception in getting the software upgrade '
                                   'status')
        return None

    db_dict = db_dict['software']
    if 'upgrade' not in db_dict:
        if 'upgrade_status' in db_dict:
            upgrade_status_json = ast.literal_eval(db_dict['upgrade_status'])
            db_dict = {'progress': upgrade_status_json}
        else:
            db_handle.logger.info('No upgrade info found')
            return None
    elif 'progress' not in db_dict['upgrade']:
        db_handle.logger.info('No upgrade progress info found in DB')
        return None
    else:
        db_dict = db_dict['upgrade']

    progress_db_dict = db_dict['progress']
    upgrade_out['file_name'] = progress_db_dict['image_name']
    if 'status' in progress_db_dict:
        upgrade_out['status'] = progress_db_dict['status']
    if 'end_timestamp_epoch' in progress_db_dict:
        upgrade_out['upgrade_timestamp'] = progress_db_dict[
            'end_timestamp_epoch']
    elif 'start_timestamp_epoch' in progress_db_dict:
        upgrade_out['upgrade_timestamp'] = progress_db_dict[
            'start_timestamp_epoch']

    upgrade_out['node_status'] = []
    if 'node' in progress_db_dict:
        for node in progress_db_dict['node']:
            node_dict = {'node_name': node, 'module_status': []}
            for k, v in progress_db_dict['node'][node].iteritems():
                if k in ['services', 'node_status', 'node_service_status',
                         'node_package_version', 'version',
                         'node_rollback_flag']:
                    continue

                if 'timestamp' in k:
                    if 'start_epoch' in v:
                        node_dict['node_start_timestamp'] = v['start_epoch']
                    if 'end_epoch' in v:
                        node_dict['node_end_timestamp'] = v['end_epoch']
                else:
                    node_dict['module_status'].append(
                        {'name': k, 'status': v})

            upgrade_out['node_status'].append(node_dict)

    return upgrade_out


@do_pre_checks
def initiate_sw_downgrade(db_handle, downgrade_image_file, force_flag):
    """
    :param db_handle: ETCD DB handle
    :param downgrade_image_file: Image file to perform downgrade
    :param force_flag: Forcefully downgrade
    :return:
        0 - Success
        1 - Invalid input
        2 - downgrade in progress
        3 - downgrade failed
        4 - ETCD DB put/get failure
        5 - Checksum mismatch
        6 - Image copy failed
        7 - Compatibility check failed
        8 - Software image up to date
        9 - Downgrade image not found
    """

    etc_dir = '/etc/nkv-agent/'
    downgrade_image_dir = etc_dir + 'upgrade_dir/'
    if not downgrade_image_file:
        # Update the etcd with the new downgrade file
        try:
            image_basename = db_handle.get_key_value('/software/prev_image')
            if not image_basename:
                db_handle.logger.info('Prev image to be rolled back to not '
                                      'found')
                return 9

            db_handle.save_multiple_key_values(
                {
                    '/software/downgrade/progress/image_dir': downgrade_image_dir,
                    '/software/downgrade/progress/image_name': image_basename,
                }
            )
        except:
            db_handle.logger.exception(
                'Exception in updating the downgrade image name to DB')
            return 4

        return 0

    image_basename = os.path.basename(downgrade_image_file)

    # Check if any downgrade going on and return error
    try:
        curr_image = db_handle.get_key_value('/software/current_image')
        if curr_image and curr_image == image_basename:
            db_handle.logger.info('Software image up to date')
            return 8
        img_in_progress = db_handle.get_key_value(
            '/software/downgrade/progress/image_name')
    except:
        db_handle.logger.exception('Exception in getting the key of '
                                   'inprogress image')
        return 4

    if img_in_progress:
        db_handle.logger.info('downgrade already in progress with image %s. '
                              'Return invalid', img_in_progress)
        return 2

    # Check the image consistency
    try:
        tarobj = tarfile.open(downgrade_image_file)
        chksum_read = None
        file_checksum_value = None
        for elem in tarobj.getmembers():
            if elem.name.endswith('md5sum'):
                chksum_read = tarobj.extractfile(elem.name).read().split()[0]
            if elem.name.endswith('tar'):
                obj = tarobj.extractfile(elem.name)
                file_checksum_value = hashlib.md5(obj.read()).hexdigest()
    except:
        db_handle.logger.exception('Exception in untar')
        return 3

    if not chksum_read:
        db_handle.logger.error('Could not read the checksum from the file %s',
                               image_basename.split('.')[0] + '.md5sum')
        return 5

    if chksum_read != file_checksum_value:
        db_handle.logger.error('Checksum mismatch, Given %s, Found %s',
                               file_checksum_value, chksum_read)
        return 5

    # Copy the file to various nodes
    try:
        cluster_info, _, _, _ = __get_cluster_info(db_handle)
    except:
        db_handle.logger.exception('Error in getting cluster member details')
        return 4

    if 'members' not in cluster_info:
        db_handle.logger.error('Wrong with the cluster info %s',
                               str(cluster_info))
        return 4

    members = cluster_info['members']
    for member in members:
        ipaddr = member['cm_server_address']
        cmd = (
            'ssh root@' + ipaddr + ' mkdir -p ' + downgrade_image_dir + '&&'
            + 'scp ' + downgrade_image_file + ' root@' + ipaddr
            + ':' + downgrade_image_dir + image_basename)
        db_handle.logger.info('Cmd to copy file %s', cmd)
        p = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = p.communicate()
        if err:
            db_handle.logger.error('Error in copying the file [%s] err %s',
                                   cmd, err)
            return 6
        if out:
            db_handle.logger.info('cmd %s, out %s', cmd, out)

    # Update the etcd with the new downgrade file
    try:
        db_handle.save_multiple_key_values(
            {
                '/software/downgrade/progress/image_dir': downgrade_image_dir,
                '/software/downgrade/progress/image_name': image_basename,
                '/software/downgrade/progress/checksum': file_checksum_value,
            }
        )
    except:
        db_handle.logger.exception('Exception in updating the downgrade image '
                                   'name to DB')
        return 4

    return 0


@do_pre_checks
def lib_get_sw_downgrade_progress(db_handle):
    downgrade_out = dict()
    try:
        db_dict = db_handle.get_key_with_prefix('/software')
    except:
        db_handle.logger.exception('Exception in getting the software downgrade '
                                   'status')
        return None

    db_dict = db_dict['software']
    if 'downgrade' not in db_dict:
        if 'downgrade_status' in db_dict:
            downgrade_status_json = ast.literal_eval(db_dict['downgrade_status'])
            db_dict = {'progress': downgrade_status_json}
        else:
            db_handle.logger.info('No downgrade info found')
            return None
    elif 'progress' not in db_dict['downgrade']:
        db_handle.logger.info('No downgrade progress info found in DB')
        return None
    else:
        db_dict = db_dict['downgrade']

    progress_db_dict = db_dict['progress']
    downgrade_out['file_name'] = progress_db_dict['image_name']
    if 'status' in progress_db_dict:
        downgrade_out['status'] = progress_db_dict['status']
    if 'end_timestamp_epoch' in progress_db_dict:
        downgrade_out['downgrade_timestamp'] = progress_db_dict[
            'end_timestamp_epoch']
    elif 'start_timestamp_epoch' in progress_db_dict:
        downgrade_out['downgrade_timestamp'] = progress_db_dict[
            'start_timestamp_epoch']

    downgrade_out['node_status'] = []
    if 'node' in progress_db_dict:
        for node in progress_db_dict['node']:
            node_dict = {'node_name': node, 'module_status': []}
            for k, v in progress_db_dict['node'][node].iteritems():
                if k in ['services', 'node_status', 'node_service_status',
                         'node_package_version', 'version',
                         'node_rollback_flag']:
                    continue

                if 'timestamp' in k:
                    if 'start_epoch' in v:
                        node_dict['node_start_timestamp'] = v['start_epoch']
                    if 'end_epoch' in v:
                        node_dict['node_end_timestamp'] = v['end_epoch']
                else:
                    node_dict['module_status'].append(
                        {'name': k, 'status': v})

            downgrade_out['node_status'].append(node_dict)

    return downgrade_out
