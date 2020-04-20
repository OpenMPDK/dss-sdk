# External imports
from copy import deepcopy

# System template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#ComputerSystem.ComputerSystem",
        "@odata.id": "{rest_base}Systems/{sys_id}",
        "@odata.type": "#ComputerSystem.v1_9_0.ComputerSystem",
        "Description": "System representing the drive resources",
        "Id": "{sys_id}",
        "Identifiers": [
            {
                "DurableName": "{nqn_id}",
                "DurableNameFormat": "NQN"
            }
        ],
        "Status": {
            "State": "Enabled",
            "Health": "OK"
        },
        "Storage": {
            "@odata.id": "{rest_base}Systems/{sys_id}/Storage"
        },
        "EthernetInterfaces": {
            "@odata.id": "{rest_base}Systems/{sys_id}/EthernetInterfaces"
        },
        "Name": "System",
        "oem": {
            "ServerName": "msl-ssg-mp03",
            "NSID": "{ns_id}",
            "NumaAligned": False
        },
        "Links": {
            "ConsumingComputerSystems": [
                {
                    "@odata.id": "{rest_base}Systems/Targetuuid{nqn_id}"
                }
            ]
        }
    }


def get_System_instance(wildcards):
    """
    Instantiate system template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    config['Identifiers'][0]['DurableName'] = config['Identifiers'][0]['DurableName'].format(
        **wildcards)
    config['Storage']['@odata.id'] = config['Storage']['@odata.id'].format(
        **wildcards)
    config['EthernetInterfaces']['@odata.id'] = config['EthernetInterfaces']['@odata.id'].format(
        **wildcards)
    config['oem']['NSID'] = config['oem']['NSID'].format(**wildcards)
    config['Links']['ConsumingComputerSystems'][0]['@odata.id'] = config[
        'Links']['ConsumingComputerSystems'][0]['@odata.id'].format(**wildcards)
    return config
