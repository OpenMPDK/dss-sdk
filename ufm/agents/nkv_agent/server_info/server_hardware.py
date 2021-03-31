"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import netifaces
import os
import re
import yaml

from subprocess import Popen, PIPE

default_filename = 'system_info.yml'


def read_file_contents(path):
    buf = None
    try:
        with open(path) as fh:
            buf = str((fh.read().rstrip()))
    finally:
        return buf


class ServerNetwork:
    def __init__(self):
        self.pci_ids = self.get_pci_ids()
        self.pci_ids_dict = None
        if self.pci_ids is not None:
            self.pci_ids_dict = self.pci_ids_to_dict(self.pci_ids)

    @staticmethod
    def get_driver_name(name):
        path = '/sys/class/net/' + name + '/device/driver'
        driver_path = os.path.realpath(path)
        pos = driver_path.rfind('/') + 1
        driver_name = driver_path[pos:]
        return driver_name

    @staticmethod
    def get_driver_version(driver_name):
        path = '/sys/module/' + driver_name + '/version'
        driver_version = read_file_contents(path)
        if driver_version is None:
            return None
        else:
            return driver_version

    @staticmethod
    def get_numa(name):
        path = '/sys/class/net/' + name + '/device/numa_node'
        numa_node = read_file_contents(path)
        if numa_node is None:
            return -1
        else:
            return numa_node

    @staticmethod
    def get_duplex(name):
        path = '/sys/class/net/' + name + '/duplex'
        duplex = read_file_contents(path)
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
        vendor_id = None
        for line in lines:
            m = re.search('^(\s*)([0-9a-fA-F]{4})\s{2}(.+)', line)
            if m is not None:
                if m.group(1) == '':
                    vendor_id = m.group(2)
                    vendor_str = m.group(3)
                    pci_ids_dict[vendor_id] = {"vendor_str": vendor_str,
                                               "devices": {}}
                else:
                    device_id = m.group(2)
                    device_str = m.group(3)
                    if vendor_id:
                        pci_ids_dict[vendor_id]["devices"][device_id] = device_str
        return pci_ids_dict

    @staticmethod
    def get_vendor_id(name):
        path = '/sys/class/net/' + name + '/device/vendor'
        vendor_id = read_file_contents(path)
        if vendor_id is None:
            return None
        else:
            return vendor_id[2:]

    @staticmethod
    def get_speed(name):
        path = '/sys/class/net/' + name + '/speed'
        speed = read_file_contents(path)
        if speed is None:
            return None
        else:
            return speed

    @staticmethod
    def get_device_id(name):
        path = '/sys/class/net/' + name + '/device/device'
        device_id = read_file_contents(path)
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
            % name, os.path.realpath(nic_path))
        if m is not None:
            pci_addr = m.group(1)
        else:
            pci_addr = ""
        return pci_addr

    def identify_networkinterfaces(self):
        interfaces = netifaces.interfaces()
        iface_filtered = {}
        for iface in interfaces:
            iface = str(iface)
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

            mac_addr = str(iface_info[netifaces.AF_LINK][0]["addr"])
            if mac_addr not in iface_filtered:
                iface_filtered[mac_addr] = {}
                iface_filtered[mac_addr]["NUMANode"] = self.get_numa(phys_interface)
                iface_filtered[mac_addr]["Duplex"] = self.get_duplex(phys_interface)
                iface_filtered[mac_addr]["Speed"] = self.get_speed(phys_interface)
                iface_filtered[mac_addr]["MACAddress"] = mac_addr
                iface_filtered[mac_addr]["InterfaceName"] = str(iface)
                iface_filtered[mac_addr]["Vendor"] = str(vendor)
                iface_filtered[mac_addr]["Device"] = str(device)
                iface_filtered[mac_addr]["PCIAddress"] = str(self.get_pci_address(phys_interface))
                iface_filtered[mac_addr]["Driver"] = str(driver_name)
                iface_filtered[mac_addr]["DriverVersion"] = \
                    self.get_driver_version(driver_name)
                iface_filtered[mac_addr]["Interfaces"] = {}

            iface_list = iface_filtered[mac_addr]['Interfaces']
            iface_list[iface] = {}

            # For now, only mark devices having IPv4 address as UP
            if netifaces.AF_INET in iface_info:
                ipv4 = iface_info[netifaces.AF_INET][0]["addr"]
                iface_list[iface]["IPv4"] = str(ipv4)
            if netifaces.AF_INET6 in iface_info:
                ipv6 = iface_info[netifaces.AF_INET6][0]["addr"].split('%')[0]
                iface_list[iface]["IPv6"] = str(ipv6)

            iface_filtered[mac_addr]['Interfaces'] = iface_list
        return iface_filtered


class ServerHardware(object):
    def __init__(self):
        self.record_re = re.compile(r"\t(.+):\s+(.+)$")
        self.dmidecode_cmd = '/sbin/dmidecode'
        pass

    def process_cmd(self, cmd, record_id):
        item_list = []
        try:
            proc = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
            out, err = proc.communicate()
        except Exception as e:
            print('Exception in running cmd %s, exception %s' % (cmd, str(e)))
            return item_list

        if err:
            print('Error in getting dmidecode details for cmd %s' % cmd)
            return item_list

        item_info = None
        for line in out.splitlines():
            line = line.decode('utf-8')
            if line.startswith('Handle'):
                if item_info:
                    item_list.append(item_info)
                item_info = {}
            else:
                m = self.record_re.findall(line)
                if not m:
                    continue
                if m[0][0] in record_id:
                    item_info[str(m[0][0])] = str(m[0][1]).strip()

        if item_info:
            item_list.append(item_info)

        return item_list

    def get_cpu_info(self):
        cmd = self.dmidecode_cmd + ' -t processor'
        record_id = ['Socket Designation', 'Manufacturer', 'Family', 'Version', 'Status', 'Serial Number',
                     'Port Number', 'Core Count', 'Core Enabled', 'Thread Count', 'Max Speed', 'Current Speed']
        cpu_list = self.process_cmd(cmd, record_id)
        return cpu_list

    def get_memory_info(self):
        cmd = self.dmidecode_cmd + ' -t memory'
        record_id = ['Size', 'Form Factor', 'Manufacturer', 'Locator', 'Bank Locator', 'Type', 'Serial Number',
                     'Part Number', 'Type', 'Configured Clock Speed']
        tmp_list = self.process_cmd(cmd, record_id)
        mem_list = []
        for elem in tmp_list:
            if 'Size' in elem and elem['Size'] != 'No Module Installed':
                mem_list.append(elem)
        return mem_list

    def get_bios_info(self):
        cmd = self.dmidecode_cmd + ' -t bios'
        record_id = ['Vendor', 'Version', 'BIOS Revision', 'Release Date']
        bios_info = self.process_cmd(cmd, record_id)
        return bios_info

    def get_chassis_info(self):
        cmd = self.dmidecode_cmd + ' -t chassis'
        record_id = ['Boot-up State', 'Power Supply State', 'Thermal State']
        chassis_info = self.process_cmd(cmd, record_id)
        return chassis_info

    @staticmethod
    def get_numa_map():
        numa_map = {}
        root_dir = '/sys/devices/system/node'
        if not os.access(root_dir, os.X_OK):
            return
        for root, dirs, files in os.walk(root_dir):
            if not root.startswith(os.path.join(root_dir, 'node')):
                continue
            if 'cpulist' in files:
                node = root.split('/')[-1]
                numa_map[node] = {}
                with open(os.path.join(root, 'cpulist')) as fh:
                    numa_map[node]['cpu_list'] = fh.readline().strip()
                with open(os.path.join(root, 'distance')) as fh:
                    numa_map[node]['distance'] = fh.readline().strip()

        return numa_map

    def get_network_info(self):
        sn = ServerNetwork()
        ifaces = sn.identify_networkinterfaces()
        return ifaces


class MellanoxInfo(object):
    def __init__(self):
        pass

    def get_mellanox_qos(self, ifaces):
        qos_details = {}
        for mac in ifaces:
            iface = ifaces[mac]
            iface_name = iface['InterfaceName']
            if 'Mellanox' in iface['Vendor']:
                cmd = 'mlnx_qos -i ' + iface_name
                try:
                    proc = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
                    out, err = proc.communicate()
                except Exception as e:
                    print('Exception in running cmd %s, exception %s' % (cmd, str(e)))
                    continue

                if err:
                    print('Error in getting details for cmd %s' % cmd)
                    continue

                qos_details[iface_name] = {}
                lineiter = iter(out.splitlines())
                for line in lineiter:
                    line = line.decode('utf-8')
                    if 'WARNING' in line:
                        continue
                    if 'PFC configuration' in line:
                        if 'pfc' not in qos_details[iface_name]:
                            qos_details[iface_name]['pfc'] = {}
                        for i in range(3):
                            line = next(lineiter)
                            line = line.decode('utf-8')
                            line = line.strip('\t')
                            v = str(line).split(None, 1)
                            qos_details[iface_name]['pfc'][v[0]] = v[1]
                    if 'tc' in line:
                        if 'tc' not in qos_details[iface_name]:
                            qos_details[iface_name]['tc'] = {}
                        line = str(line)
                        v = line.split(' ', 2)
                        qos_details[iface_name]['tc'][v[1]] = v[2]
                        next(lineiter)

        return qos_details

    def get_mellanox_config(self):
        mlnx_config = {}
        sys_class_path = '/sys/class/infiniband'
        for root, dirs, _ in os.walk(sys_class_path):
            for d in dirs:
                cmd = 'mlxconfig -d ' + d + ' q'
                try:
                    proc = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
                    out, err = proc.communicate()
                except Exception as e:
                    print('Exception in running cmd %s, exception %s' % (cmd, str(e)))
                    continue

                if err:
                    print('Error in getting details for cmd %s' % cmd)
                    continue

                mlnx_config[d] = {}
                config_str_found = False
                lineiter = iter(out.splitlines())
                for line in lineiter:
                    line = line.decode('utf-8')
                    if 'Configurations' in line:
                        config_str_found = True

                    if not config_str_found:
                        continue
                    v = str(line).split()
                    mlnx_config[d][v[0]] = v[1]

        return mlnx_config


def collect_system_info(system_info_file=default_filename):
    system_info = {}
    sh = ServerHardware()
    cpu = sh.get_cpu_info()
    system_info['CPU'] = cpu
    mem = sh.get_memory_info()
    system_info['Memory'] = mem
    bios = sh.get_bios_info()
    system_info['BIOS'] = bios
    chassis = sh.get_chassis_info()
    system_info['Chassis'] = chassis
    numa_info = sh.get_numa_map()
    system_info['NUMA Info'] = numa_info
    net_ifaces = sh.get_network_info()
    system_info['Network'] = net_ifaces
    mlnx_info = MellanoxInfo()
    system_info['NIC QOS'] = mlnx_info.get_mellanox_qos(net_ifaces)
    system_info['NIC Configuration'] = mlnx_info.get_mellanox_config()

    with open(system_info_file, 'w') as f:
        yaml.dump(system_info, f, default_flow_style=False)


if __name__ == '__main__':
    collect_system_info()