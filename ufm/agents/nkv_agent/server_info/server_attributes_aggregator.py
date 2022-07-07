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


import base64
import json
import math
import os
import re
import socket
import struct
# import subprocess
import sys
import uuid
import zlib
from glob import glob
from os.path import basename, dirname, realpath

import netifaces

from device_ioctl import ioctl_nvme_admin_command
from utils.log_setup import agent_logger
from utils.jsonrpc import SPDKJSONRPC
from utils.utils import read_linux_file


class OSMServerCPU:
    def __init__(self):
        self.logger = agent_logger

    def get_numa_count(self):
        buf = read_linux_file('/sys/devices/system/node/online')
        if buf is None:
            self.logger.error("Unable to determine NUMA nodes online")
            sys.exit(-1)
        else:
            m = buf.split('-')
            numa_count = int(m[-1]) + 1
        return numa_count

    def get_numa_map(self, numa_id):
        buf = read_linux_file('/sys/devices/system/node/node%d/cpulist' %
                              numa_id)
        if buf is None:
            self.logger.error("Unable to retrieve NUMA Mapping for NUMA %d" %
                              numa_id)
            sys.exit(-1)
        else:
            numa_map = buf
        return numa_map

    @staticmethod
    def get_cpu_cores():
        with open('/proc/cpuinfo') as f:
            read_buffer = f.read().rstrip('\n')
        read_buffer = re.sub('[\t]', '', read_buffer)
        cores = read_buffer.split('\n\n')
        return cores, len(cores)

    def get_all_cpu(self):
        cores, core_count = self.get_cpu_cores()
        core_info = [{}] * core_count

        # NUMA Node mapping
        numa_count = self.get_numa_count()
        numa_map = {}
        for numa_id in range(numa_count):
            numa_map[str(numa_id)] = self.get_numa_map(numa_id)

        socket_count = 0
        for idx, core in enumerate(cores):
            for line in core.split('\n'):
                tmp = line.split(':')
                if len(tmp) > 1:
                    tmp[1] = tmp[1].strip(' ')
                    core_info[idx][tmp[0]] = tmp[1]
            phys_id = core_info[idx]["physical id"]
            if int(phys_id) > socket_count:
                socket_count = int(phys_id)
        socket_count = int(socket_count) + 1  # Index to counting
        physical_count_per_core = int(core_info[0]["cpu cores"])
        physical_count = physical_count_per_core * socket_count
        cpu_metadata = {"CoreCount": core_count,
                        "SocketCount": socket_count,
                        "PhysicalCountPerCore": physical_count_per_core,
                        "PhysicalCount": physical_count,
                        "LogicalCount": int(core_count),
                        "ThreadPerCore": int(core_count) / int(physical_count),
                        "ModelName": core_info[0]["model name"],
                        "Endian": sys.byteorder,
                        "NUMACount": numa_count,
                        "NUMAMap": numa_map, }
        return cpu_metadata, core_info

    def identify_cpu(self):
        core_info_pair = self.get_all_cpu()
        cpu_info = {"LogicalCount": core_info_pair[0]["LogicalCount"],
                    "PhysicalCount": core_info_pair[0]["PhysicalCount"],
                    "SocketCount": core_info_pair[0]["SocketCount"],
                    "ThreadPerCore": int(core_info_pair[0]["ThreadPerCore"]),
                    "ModelName": core_info_pair[0]["ModelName"],
                    "Endian": core_info_pair[0]["Endian"],
                    "NUMACount": core_info_pair[0]["NUMACount"],
                    "NUMAMap": core_info_pair[0]["NUMAMap"], }
        return cpu_info

    def cpu_helper(self):
        cpu_dict = self.identify_cpu()
        return cpu_dict


class OSMServerNetwork:
    def __init__(self):
        self.pci_ids = self.get_pci_ids()
        self.pci_ids_dict = None
        if self.pci_ids is not None:
            print("Creating PCI IDs dict")
            self.pci_ids_dict = self.pci_ids_to_dict(self.pci_ids)
        self.logger = agent_logger

    @staticmethod
    def get_driver_name(name):
        path = '/sys/class/net/' + name + '/device/driver'
        driver_path = realpath(path)
        pos = driver_path.rfind('/') + 1
        driver_name = driver_path[pos:]
        return driver_name

    @staticmethod
    def get_driver_version(driver_name):
        path = '/sys/module/' + driver_name + '/version'
        driver_version = read_linux_file(path)
        if driver_version is None:
            return None
        else:
            return driver_version

    @staticmethod
    def get_numa(name):
        path = '/sys/class/net/' + name + '/device/numa_node'
        numa_node = read_linux_file(path)
        if numa_node is None:
            return -1
        else:
            return numa_node

    @staticmethod
    def get_duplex(name):
        path = '/sys/class/net/' + name + '/duplex'
        duplex = read_linux_file(path)
        if duplex is None:
            return None
        else:
            return duplex

    @staticmethod
    def get_pci_ids():
        pci_ids = None
        file_name = "/usr/share/hwdata/pci.ids"
        try:
            with open(file_name) as f:
                pci_ids = str(f.read().rstrip())
        except Exception:
            print("Missing " + file_name)
        return pci_ids

    @staticmethod
    def pci_ids_to_dict(pci_ids):
        pci_ids_dict = {}
        lines = pci_ids.split('\n')
        for line in lines:
            m = re.search(r'^(\s*)([0-9a-fA-F]{4})\s{2}(.+)', line)
            if m is not None:
                if m.group(1) == '':
                    vendor_id = m.group(2)
                    vendor_str = m.group(3)
                    pci_ids_dict[vendor_id] = {"vendor_str": vendor_str,
                                               "devices": {}}
                else:
                    device_id = m.group(2)
                    device_str = m.group(3)
                    pci_ids_dict[vendor_id]["devices"][device_id] = device_str
        return pci_ids_dict

    @staticmethod
    def get_vendor_id(name):
        path = '/sys/class/net/' + name + '/device/vendor'
        vendor_id = read_linux_file(path)
        if vendor_id is None:
            return None
        else:
            return vendor_id[2:]

    @staticmethod
    def get_speed(name):
        path = '/sys/class/net/' + name + '/speed'
        speed = read_linux_file(path)
        if speed is None:
            return None
        else:
            return speed

    # when the NIC is up/down, the operstate will always be in 'up' and not useful
    @staticmethod
    def get_nic_status(name):
        path = '/sys/class/net/' + name + '/operstate'
        status = read_linux_file(path)
        if status is None:
            return None
        else:
            return status

    @staticmethod
    def get_device_id(name):
        path = '/sys/class/net/' + name + '/device/device'
        device_id = read_linux_file(path)
        if device_id is None:
            return None
        else:
            return device_id[2:]

    def get_vendor_device(self, name):
        vendor = device = "Unknown"

        if self.pci_ids is not None:
            vendor_id = self.get_vendor_id(name)
            if vendor_id is None:
                return vendor, device
            else:
                vendor = self.pci_ids_dict[vendor_id]["vendor_str"]

            device_id = self.get_device_id(name)
            if device_id is None:
                return vendor, device
            else:
                device = self.pci_ids_dict[vendor_id]["devices"][device_id]
        return vendor, device

    @staticmethod
    def get_pci_address(name):
        nic_path = '/sys/class/net/' + name
        m = re.match(
            r"^.+?([0-9A-Fa-f]{4}(?::[0-9A-Fa-f]{2}){2}\.[0-9A-Fa-f])/net/%s$"
            % name, realpath(nic_path))
        if m is not None:
            pci_addr = m.group(1)
        else:
            pci_addr = ""
        return pci_addr

    def identify_networkinterfaces(self):
        interfaces = netifaces.interfaces()
        iface_filtered = {}
        for iface in interfaces:
            iface_info = netifaces.ifaddresses(iface)
            # For VLAN interfaces, the name of the interface can be <physical iface name>.<identifier>
            # To get the driver, vendor and other interface details, we need physical iface name
            phys_interface = iface.split('.', 1)[0]
            vendor, device = self.get_vendor_device(phys_interface)
            if vendor == "Unknown" or device == "Unknown":
                continue

            driver_name = self.get_driver_name(phys_interface)
            # if not driver_name.startswith("mlx5_"):
            #    continue

            mac_addr = iface_info[netifaces.AF_LINK][0]["addr"]
            if mac_addr not in iface_filtered:
                iface_filtered[mac_addr] = {}
                iface_filtered[mac_addr]["NUMANode"] = self.get_numa(phys_interface)
                iface_filtered[mac_addr]["Duplex"] = self.get_duplex(phys_interface)
                iface_filtered[mac_addr]["Speed"] = self.get_speed(phys_interface)
                iface_filtered[mac_addr]["MACAddress"] = mac_addr
                iface_filtered[mac_addr]["InterfaceName"] = iface
                iface_filtered[mac_addr]["Vendor"] = vendor
                iface_filtered[mac_addr]["Device"] = device
                iface_filtered[mac_addr]["PCIAddress"] = self.get_pci_address(phys_interface)
                iface_filtered[mac_addr]["Driver"] = driver_name
                iface_filtered[mac_addr]["DriverVersion"] = \
                    self.get_driver_version(driver_name)
                iface_filtered[mac_addr]["Interfaces"] = {}

            iface_list = iface_filtered[mac_addr]['Interfaces']
            iface_list[iface] = {}
            nic_status = 'down'

            # For now, only mark devices having IPv4 address as UP
            if netifaces.AF_INET in iface_info:
                ipv4 = iface_info[netifaces.AF_INET][0]["addr"]
                iface_list[iface]["IPv4"] = ipv4
                nic_status = 'up'
            iface_list[iface]["Status"] = nic_status
            if netifaces.AF_INET6 in iface_info:
                ipv6 = iface_info[netifaces.AF_INET6][0]["addr"].split(
                    '%')[0]
                iface_list[iface]["IPv6"] = ipv6
                iface_list[iface]["Status"] = 'up'

            # Check Status present or not. For first loop of interfaces, we need to initialize
            if 'Status' not in iface_filtered[mac_addr]:
                iface_filtered[mac_addr]['Status'] = nic_status
            elif iface_filtered[mac_addr]['Status'] != 'up':
                # In case of Virtual interfaces for physical NIC, then carry the nic_status to the parent in
                # case the vlan is 'up'
                iface_filtered[mac_addr]['Status'] = nic_status

            iface_filtered[mac_addr]['Interfaces'] = iface_list
            iface_filtered[mac_addr]["CRC"] = zlib.crc32(
                json.dumps(iface_filtered[mac_addr], sort_keys=True).encode())
        return iface_filtered

    def network_helper(self):
        network_dict = {"interfaces": self.identify_networkinterfaces()}
        network_dict["Count"] = len(network_dict["interfaces"])
        return network_dict


class OSMServerStorage:
    def __init__(self, recv_sz=4096, socket_path="/var/run/spdk.sock"):
        self.CTRLRS = {"sata": 0,
                       "nvme": 1}
        self.SATA = self.CTRLRS["sata"]
        self.NVME = self.CTRLRS["nvme"]
        self.recv_sz = recv_sz
        self.socket_path = socket_path
        self.logger = agent_logger

    @staticmethod
    def get_pci_numa(pci_addr):
        """
        Retrieve NVMe device NUMA node on a Linux server.
        :param pci_addr: NVMe device's PCI address to map to a NUMA node.
        :return: Number as a string which represents the NUMA node the NVMe
                 device is assigned to.
        """
        pci_numa = -1
        pci_addr_short = ':'.join(pci_addr.split(':')[:2])
        try:
            with open('/sys/class/pci_bus/' + pci_addr_short + '/device/numa_node') as f:
                pci_numa = str(int(f.read().rstrip()))
        finally:
            return pci_numa

    @staticmethod
    def get_storage_device_names():
        drive_glob = '/sys/block/*/device'
        return glob(drive_glob)

    @staticmethod
    def get_firmware_rev(name):
        path = '/sys/block/' + name + '/device/firmware_rev'
        firmware_rev = read_linux_file(path)
        if firmware_rev is None:
            return ""
        else:
            return firmware_rev

    @staticmethod
    def get_logical_block_size(name):
        path = '/sys/block/' + name + '/queue/logical_block_size'
        logical_blk_sz = read_linux_file(path)
        if logical_blk_sz is None:
            return -1
        else:
            return logical_blk_sz

    @staticmethod
    def get_size(name):
        path = '/sys/block/' + name + '/size'
        size = read_linux_file(path)
        if size is None:
            return -1
        else:
            return size

    @staticmethod
    def decode_base64(data):
        """Decode base64, padding being optional.

        :param data: Base64 data as an ASCII byte string
        :returns: The decoded byte string.

        """
        missing_padding = len(data) % 4
        if missing_padding != 0:
            data += b'=' * (4 - missing_padding)
        return base64.decodestring(data)

    @staticmethod
    def unpack_identify_ctrlr_details(data, d):
        regex = re.compile(r"(?:\x00| )+")
        d["Serial"] = re.sub(regex, '', ''.join(
            struct.unpack_from('20c', data, offset=4)))

    def unpack_identify_ns_details(self, data, d):
        if "LogicalBlockSize" not in d:
            self.logger.error("Could not find block size")
            return

        block_size = int(d["LogicalBlockSize"])
        disk_capacity, disk_utilization_block = struct.unpack_from('QQ', data, offset=8)
        d["DiskCapacityInBytes"] = disk_capacity * block_size
        d["DiskUtilizationInBytes"] = disk_utilization_block * block_size
        d["DiskUtilizationPercentage"] = "%.2f" % (float(disk_utilization_block * 100.0) / disk_capacity)

    @staticmethod
    def unpack_smart_details(data, d):
        resp_fields_format = [('crit_warn', 'B', 0),
                              ('temp', 'H', 1),
                              ('data_units_read', 'QQ', 32),
                              ('data_units_written', 'QQ', 48),
                              ('host_read_commands', 'QQ', 64),
                              ('host_write_commands', 'QQ', 80),
                              ('power_cycles', 'QQ', 112),
                              ('power_on_hours', 'QQ', 128),
                              ('unsafe_shutdowns', 'QQ', 144),
                              ('media_errors', 'QQ', 160),
                              ('num_err_log_entries', 'QQ', 176)]
        fields = {}
        for item in resp_fields_format:
            fields[item[0]] = struct.unpack_from(item[1], data, item[2])

        d["DriveCriticalWarning"] = fields['crit_warn'][0]
        # Temperature is reported in Kevin. Convert to Celsisus
        d["DriveTemperature"] = int(math.ceil(fields['temp'][0] - 273.15))
        # Each of the below units is 16 bytes. We are getting tuple of 8
        # bytes each. Merge them
        d["DriveDataUnitsRead"] = (fields['data_units_read'][1] << 64
                                   | fields['data_units_read'][0])
        d["DriveDataUnitsWritten"] = (fields['data_units_written'][1] << 64
                                      | fields['data_units_written'][0])
        d["DriveHostReadCommands"] = (fields['host_read_commands'][1] << 64
                                      | fields['host_read_commands'][0])
        d["DriveHostWriteCommands"] = (fields['host_write_commands'][1] << 64
                                       | fields['host_write_commands'][0])
        d["DrivePowerCycles"] = (fields['power_cycles'][1] << 64
                                 | fields['power_cycles'][0])
        d["DrivePowerOnHours"] = (fields['power_on_hours'][1] << 64
                                  | fields['power_on_hours'][0])
        d["DriveUnsafeShutdowns"] = (fields['unsafe_shutdowns'][1] << 64
                                     | fields['unsafe_shutdowns'][0])
        d["DriveMediaErrors"] = (fields['media_errors'][1] << 64
                                 | fields['media_errors'][0])
        d["DriveNumErrLogEntries"] = (fields['num_err_log_entries'][1] << 64
                                      | fields['num_err_log_entries'][0])

    def get_identify_ctrlr_details(self, device_path, d, spdk_flag=0):
        data = None
        if spdk_flag:
            payload = SPDKJSONRPC.build_payload('dfly_nvme_passthru',
                                                {'opcode': 0x06, 'nsid': 0,
                                                 'data_len': 5120, 'cdw10': 1,
                                                 'device_name': device_path, })
            results_identify_ctrlr = SPDKJSONRPC.call(payload, self.recv_sz,
                                                      self.socket_path)
            if "result" in results_identify_ctrlr:
                identify_ctrlr = results_identify_ctrlr["result"][0]
                identify_ctrlr_data = identify_ctrlr["payload"]
                data = self.decode_base64(identify_ctrlr_data)
        else:
            data = ioctl_nvme_admin_command(device_path, 0x06, 0, 4096, 1)

        if data is not None:
            self.unpack_identify_ctrlr_details(data, d)

    def get_identify_ns_details(self, device_path, d, spdk_flag=0):
        data = None
        if spdk_flag:
            payload = SPDKJSONRPC.build_payload('dfly_nvme_passthru',
                                                {'opcode': 0x06, 'nsid': 1,
                                                 'data_len': 5120, 'cdw10': 0,
                                                 'device_name': device_path, })
            results_identify_ns = SPDKJSONRPC.call(payload, self.recv_sz, self.socket_path)
            if "result" in results_identify_ns:
                identify_ns = results_identify_ns["result"][0]
                identify_ns_data = identify_ns["payload"]
                data = self.decode_base64(identify_ns_data)
        else:
            data = ioctl_nvme_admin_command(device_path, 0x06, 1, 4096, 0)

        if data is not None:
            self.unpack_identify_ns_details(data, d)

    def get_smart_details(self, device_path, d, spdk_flag=0):
        data = None
        if spdk_flag:
            payload = SPDKJSONRPC.build_payload('dfly_nvme_passthru',
                                                {'opcode': 0x02,
                                                 'nsid': 0xffffffff,
                                                 'data_len': 512,
                                                 'cdw10': 0x007f0002,
                                                 'device_name': device_path, })
            results_smart = SPDKJSONRPC.call(payload, self.recv_sz,
                                             self.socket_path)
            if "result" in results_smart:
                smart = results_smart["result"][0]
                smart_data = smart["payload"]
                data = self.decode_base64(smart_data)
        else:
            data = ioctl_nvme_admin_command(device_path, 0x02,
                                            0xffffffff, 512, 0x007f0002)

        if data is not None:
            self.unpack_smart_details(data, d)

    def get_storage_devices(self, dev_type):
        physical_drives = {}

        if dev_type == self.SATA:
            pattern = re.compile(r"^sd[b-z]$")
        elif dev_type == self.NVME:
            pattern = re.compile(r"^nvme\d+n\d+$")
        else:
            print("Error: Invalid device type to identify")
            sys.exit(-1)

        storage_devices = self.get_storage_device_names()
        for d in storage_devices:
            drive_name = basename(dirname(d))
            if pattern.match(drive_name):
                drive_info = {}
                sys_block_path = "/sys/block/" + drive_name
                drive_info["DeviceNode"] = "/dev/" + drive_name

                if dev_type == self.NVME:
                    real_path = realpath(sys_block_path)
                    if 'pci' not in real_path:
                        continue
                    m = re.match(r"^.+?([0-9A-Fa-f]{4}(?::[0-9A-Fa-f]{2}){2}\.0)/nvme/nvme\d+/nvme\d+n\d+$",
                                 real_path)
                    if not m:
                        continue
                    drive_info["PCIAddress"] = m.group(1)

                try:
                    drive_info["Model"] = str(open(sys_block_path + "/device/model").read().rstrip('\r\n'))
                except IOError:
                    drive_info["Model"] = ""
                except Exception as e:
                    print(e)
                    raise

                drive_info["FirmwareRevision"] = self.get_firmware_rev(drive_name)
                drive_info["LogicalBlockSize"] = self.get_logical_block_size(drive_name)
                drive_size = self.get_size(drive_name)
                if drive_size > 0 and drive_info["LogicalBlockSize"] > 0:
                    drive_info["SizeInBytes"] = int(drive_size) * int(drive_info["LogicalBlockSize"])
                else:
                    drive_info["SizeInBytes"] = -1

                serial = None
                if dev_type == self.NVME:
                    drive_info["NUMANode"] = self.get_pci_numa(drive_info["PCIAddress"])
                    self.get_identify_ctrlr_details("/dev/" + drive_name, drive_info, 0)
                    serial = drive_info["Serial"]
                elif dev_type == self.SATA:
                    self.get_identify_ctrlr_details("/dev/" + drive_name, drive_info, 0)
                    serial = drive_info["Serial"]
                if serial is None:
                    print("Skipping %s, no serial available" % drive_name)
                    continue
                drive_info["Serial"] = serial
                try:
                    self.get_identify_ns_details("/dev/%s" % drive_name, drive_info)
                    self.get_smart_details("/dev/%s" % drive_name, drive_info)
                except Exception:
                    pass
                physical_drives[serial] = drive_info
                physical_drives[serial]["CRC"] = zlib.crc32(json.dumps(drive_info, sort_keys=True).encode())

        results = []
        payload = SPDKJSONRPC.build_payload('get_bdevs')
        try:
            results = SPDKJSONRPC.call(payload, self.recv_sz, self.socket_path)
        except Exception:
            pass
        if 'result' in results:
            devices = results['result']
            for device in devices:
                drive_info = {}
                serial = device["driver_specific"]["nvme"]["ctrlr_data"]["serial_number"]
                if serial not in physical_drives:
                    # drive_info["DeviceNode"] = "N/A"
                    drive_info["FirmwareRevision"] = device["driver_specific"]["nvme"]["ctrlr_data"][
                        "firmware_revision"]
                    drive_info["Model"] = device["driver_specific"]["nvme"]["ctrlr_data"]["model_number"]
                    block_size = device["block_size"]
                    drive_info["LogicalBlockSize"] = block_size
                    drive_info["SizeInBytes"] = device["num_blocks"] * drive_info["LogicalBlockSize"]
                    drive_info["PCIAddress"] = device["driver_specific"]["nvme"]["pci_address"]
                    drive_info["NUMANode"] = self.get_pci_numa(drive_info["PCIAddress"])
                    drive_info["Serial"] = serial
                    try:
                        self.get_identify_ns_details("%sn1" % serial, drive_info, 1)
                        self.get_smart_details("%sn1" % serial, drive_info, 1)
                    except Exception:
                        pass
                    physical_drives[serial] = drive_info
                    physical_drives[serial]["CRC"] = zlib.crc32(json.dumps(drive_info, sort_keys=True).encode())
        return physical_drives

    def get_storage_db_dict(self):
        devices = self.get_storage_devices(self.NVME)

        capacity = 0
        for dev in devices:
            capacity += int(devices[dev]["SizeInBytes"])

        return {
            "nvme": {
                "devices": devices,
                "Count": len(devices),
                "Capacity": capacity
            },
            "Capacity": capacity,
            "Count": len(devices)
        }


class OSMServerIdentity:
    def __init__(self):
        self.logger = agent_logger

    @staticmethod
    def get_numa_count():
        numa_path = '/sys/devices/system/node/'
        for item in os.listdir(numa_path):
            if os.path.isdir(os.path.join(numa_path, item)) and re.search(r'^node\d+$', item):
                print(item)

    @staticmethod
    def get_product_uuid():
        with open('/sys/class/dmi/id/product_uuid') as f:
            product_uuid = f.read().rstrip()
        return product_uuid

    def get_machine_id(self):
        with open('/etc/machine-id') as f:
            machine_id = f.read().rstrip()
        return machine_id

    def get_machine_uuid(self):
        machine_id = self.get_machine_id()
        server_uuid = str(uuid.uuid3(uuid.NAMESPACE_DNS, machine_id))
        return server_uuid

    def get_encrypted_machine_id(self):
        id = self.get_machine_id()
        encrypt_id = base64.encodestring(id)
        return encrypt_id

    def get_decrypted_machine_id(self, encrypt_id):
        decrypt_id = base64.decodestring(encrypt_id)
        return decrypt_id

    def identify_server(self):
        server_info = {"Hostname": socket.gethostname(),
                       "UUID": self.get_machine_uuid(),
                       "MachineID": self.get_encrypted_machine_id()}
        return server_info

    def server_identity_helper(self):
        server_identity_dict = self.identify_server()
        return server_identity_dict
