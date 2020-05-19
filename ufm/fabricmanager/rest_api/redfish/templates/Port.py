# External imports
from copy import deepcopy

# Port template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#Port.Port",
        "@odata.id": "{rest_base}/Fabrics/{fab_id}/Switches/{switch_id}/Ports/{port_id}",
        "@odata.type": "#Port.v1_2_1.Port",
        "Description": "Port information",
        "Name": "Switch Port Information",
        "Id": "{port_id}",
    }


def get_port_instance(wildcards):
    """
    Instantiate drive template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    return config
