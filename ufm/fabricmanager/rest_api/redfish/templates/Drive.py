# External imports
from copy import deepcopy

# Drive template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#Drive.Drive",
        "@odata.id": "{rest_base}/Systems/{sys_id}/Storage/{storage_id}/Drives/{drive_id}",
        "@odata.type": "#Drive.v1_8_0.Drive",
        "Description": "Drive information",
        "Name": "Storage Drive Information",
        "Id": "{drive_id}",
        "BlockSizeBytes": 512,
        "CapacityBytes": 3840755982336,
        "Manufacturer": "Samsung",
        "MediaType": "SSD",
        "Model": "MZQLB3T8HALS-000AZ",
        "Protocol": "NVMeOverFabrics",
        "Revision": "ETA51KB3",
        "SerialNumber": "{drive_id}"
    }


def get_drive_instance(wildcards):
    """
    Instantiate drive template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    config['SerialNumber'] = config['SerialNumber'].format(**wildcards)
    return config
