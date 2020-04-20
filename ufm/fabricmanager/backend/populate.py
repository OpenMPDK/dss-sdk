# Internal imports
import config
from rest_api.redfish.System_api import CreateSystem
from rest_api.redfish.Storage import CreateStorage
from rest_api.redfish.EthernetInterface import CreateEthernetInterface
from rest_api.redfish.Drive import CreateDrive


def create_storage_resources(storage_template, drive_ids, system, storage_id):
    """
    Create drive resources that will be included in storage information
    """

    drive_count = 0
    for drive in storage_template['Drives']:
        for k in range(drive.get('Count', 1)):
            drive_count += 1
            drive_id = drive['Id'].format(drive_count)
            CreateDrive(rest_base='/redfish/v1', sys_id=system,
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
            CreateStorage(rest_base='/redfish/v1/', sys_id=system,
                          storage_id=storage_id, serial_numbers=drive_ids)

    for ethernet_interface in system_template['EthernetInterfaces']:
        for k in range(ethernet_interface.get('Count', 1)):
            ethernet_interface_count += 1
            nic_id = ethernet_interface['Id'].format(ethernet_interface_count)
            CreateEthernetInterface(
                rest_base='/redfish/v1/', sys_id=system, nic_id=nic_id)


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
            CreateSystem(resource_class_kwargs={
                         'rest_base': '/redfish/v1/'}).put(system, sys_count, sys_count)
            create_system_resources(system_template, system)
