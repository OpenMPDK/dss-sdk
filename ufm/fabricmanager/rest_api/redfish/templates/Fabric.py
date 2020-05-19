# External imports
from copy import deepcopy

# Fabric template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#Fabirc.Fabric",
        "@odata.id": "{rest_base}Fabrics/{fab_id}",
        "@odata.type": "#Fabric.v1_1_1.Fabric",
        "Description": "Fabric Collection",
        "Id": "{fab_id}",
        "Status": {
            "State": "Enabled",
            "Health": "OK"
        },
        "Switches": {
            "@odata.id": "{rest_base}Fabrics/{fab_id}/Switches"
        },
        "Name": "Fabric",
    }


def get_Fabric_instance(wildcards):
    """
    Instantiate fabric template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    config['Switches']['@odata.id'] = config['Switches']['@odata.id'].format(
        **wildcards)
    return config
