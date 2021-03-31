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

# Internal imports


import config
from rest_api.redfish.System_api import CreateSystemEmulation
from rest_api.redfish.Storage import CreateStorageEmulation
from rest_api.redfish.EthernetInterface import CreateEthernetInterfaceEmulation
from rest_api.redfish.Drive import CreateDriveEmulation

from rest_api.redfish.Fabric_api import CreateFabricEmulation
from rest_api.redfish.Switch import CreateSwitchEmulation
from rest_api.redfish.Port import CreatePortEmulation
from rest_api.redfish.VLAN import CreateVlanEmulation

def create_storage_resources(storage_template, drive_ids, system, storage_id):
    """
    Create drive resources that will be included in storage information
    """

    drive_count = 0
    for drive in storage_template['Drives']:
        for k in range(drive.get('Count', 1)):
            drive_count += 1
            drive_id = drive['Id'].format(drive_count)
            CreateDriveEmulation(rest_base='/redfish/v1', sys_id=system,
                                 storage_id=storage_id, drive_id=drive_id)
            drive_ids.append(drive_id)


def create_system_resources(system_template, system):
    """
    Create storage and ethernet interface resources that will be included in system information
    """

    storage_count = 0
    ethernet_interface_count = 0
    for storage in system_template['Storage']:
        for j in range(storage.get('Count', 1)):
            storage_count += 1
            storage_id = storage['Id'].format(storage_count)
            drive_ids = []
            create_storage_resources(storage, drive_ids, system, storage_id)
            CreateStorageEmulation(rest_base='/redfish/v1/', sys_id=system,
                                   storage_id=storage_id, serial_numbers=drive_ids)

    for ethernet_interface in system_template['EthernetInterfaces']:
        for k in range(ethernet_interface.get('Count', 1)):
            ethernet_interface_count += 1
            nic_id = ethernet_interface['Id'].format(ethernet_interface_count)
            CreateEthernetInterfaceEmulation(
                rest_base='/redfish/v1/', sys_id=system, nic_id=nic_id)


def create_switch_resources(switch_template, fabric, switch):
    """
    Create port and vlan resources that will be included in switch information
    """
    port_count = 0
    vlan_count = 0
    for port in switch_template['Ports']:
        for j in range(port.get('Count', 1)):
            port_count += 1
            port_id = port['Id'].format(port_count)
            CreatePort(rest_base='/redfish/v1/', fab_id=fabric,
                       switch_id=switch, port_id=port_id)

    for vlan in switch_template['VLANs']:
        for k in range(vlan.get('Count', 1)):
            vlan_count += 1
            vlan_id = vlan['Id'].format(vlan_count)
            CreateVLAN(rest_base='/redfish/v1/', fab_id=fabric,
                       switch_id=switch, vlan_id=vlan_id)


def create_fabric_resources(fabric_template, fabric):
    """
    Create switch resources that will be included in fabric
    """

    switch_count = 0
    for switch in fabric_template['Switches']:
        for j in range(switch.get('Count', 1)):
            switch_count += 1
            switch_id = switch['Id'].format(switch_count)
            create_switch_resources(switch, fabric, switch_id)
            CreateSwitch(rest_base='/redfish/v1/', fab_id=fabric,
                         switch_id=switch_id)


def populate(config):
    """
    Instantiate and populate local data from local-config.json
    """

    sys_count = 0
    sys_ids = []

    # Create system resource(s)
    for system_template in config['Systems']:
        for i in range(system_template.get('Count', 1)):
            sys_count += 1
            system = system_template['Id'].format(sys_count)
            sys_ids.append(system)
            CreateSystemEmulation(resource_class_kwargs={
                         'rest_base': '/redfish/v1/'}).put(system, sys_count, sys_count)
            create_system_resources(system_template, system)


    fab_count = 0
    fab_ids = []

    # Create fabric resource(s)
    for fabric_template in config['Fabrics']:
        for j in range(fabric_template.get('Count', 1)):
            fab_count += 1
            fabric = fabric_template['Id'].format(fab_count)
            fab_ids.append(fabric)
            CreateFabric(resource_class_kwargs={
                         'rest_base': '/redfish/v1/'}).put(fabric, fab_count, fab_count)
            create_fabric_resources(fabric_template, fabric)

