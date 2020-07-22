# External imports
from copy import deepcopy

# VLAN template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#VLAN.VLAN",
        "@odata.id": "{rest_base}/Fabrics/{fab_id}/Swtiches/{switch_id}/VLANs/{vlan_id}",
        "@odata.type": "#VLAN.v1_1_5.VLAN",
        "Description": "VLAN information",
        "Name": "Switch VLAN Information",
        "Id": "{vlan_id}",
    }


def get_vlan_instance(wildcards):
    """
    Instantiate vlan template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    return config
