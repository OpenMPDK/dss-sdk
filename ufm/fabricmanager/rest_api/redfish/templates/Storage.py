# External imports
from copy import deepcopy

# Storage template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#Storage.Storage",
        "@odata.id": "{rest_base}Systems/{sys_id}/Storage/{storage_id}",
        "@odata.type": "#Storage.v1_8_0.Storage",
        "Id": "{storage_id}",
        "Description": "Storage information",
        "Name": "Storage"
    }

_DRIVE_TEMPLATE = \
    {
        "@odata.id": "{rest_base}Systems/{sys_id}/Storage/{storage_id}/Drives/"
    }


def get_storage_instance(wildcards):
    """
    Instantiate storage template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    drives = []
    for i in range(len(wildcards['serial_numbers'])):
        drive = deepcopy(_DRIVE_TEMPLATE)
        drive['@odata.id'] = drive['@odata.id'].format(
            **wildcards) + wildcards['serial_numbers'][i]
        drives.append(drive)
    config['Drives'] = drives
    return config
