# External imports
from copy import deepcopy

# Switch template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#Switch.Switch",
        "@odata.id": "{rest_base}Fabrics/{fab_id}/Switches/{switch_id}",
        "@odata.type": "#Switch.v1_3_1.Switch",
        "Id": "{switch_id}",
        "Description": "Ethernet Switch information",
        "Name": "Switch",
        "SerialNumber": "MT1935J01956",
        "UUID": "f1ec15f8-c832-11e9-8000-b8599f784980",
        "Model": "x86onie",
        "Ports": {
            "@odata.id": "{rest_base}Fabrics/{fab_id}/Switches/{switch_id}/Ports"
        },
        "VLANs": {
            "@odata.id": "{rest_base}Fabrics/{fab_id}/Switches/{switch_id}/VLANs"
        },
    }


def get_switch_instance(wildcards):
    """
    Instantiate switch template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    config['Ports']['@odata.id'] = config['Ports']['@odata.id'].format(
        **wildcards)
    config['VLANs']['@odata.id'] = config['VLANs']['@odata.id'].format(
        **wildcards)
    return config
