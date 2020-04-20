# External imports
from copy import deepcopy

# EthernetInterface template
_TEMPLATE = \
    {
        "@odata.context": "{rest_base}$metadata#EthernetInterface.EthernetInterface",
        "@odata.id": "{rest_base}Systems/{sys_id}/EthernetInterfaces/{nic_id}",
        "@odata.type": "#EthernetInterface.v1_5_1.EthernetInterface",
        "Id": "{nic_id}",
        "Description": "Ethernet Interface information",
        "Name": "NIC",
        "MACAddress": "ac:1f:6b:cf:1d:dc",
        "SpeedMbps": 100000,
        "LinkStatus": "LinkUp",
        "IPv4Addresses": [
            {
                "Address": "102.100.20.3",
                "SubnetMask": "",
                "AddressOrigin": "",
                "Gateway": "",
                "oem": {
                    "Port": 1024,
                    "SupportedProtocol": "TCP"
                }
            }
        ],
        "IPv6DefaultGateway": "",
        "IPv6Addresses": [
            {
                "Address": "",
                "PrefixLength": 0,
                "AddressOrigin": "",
                "AddressState": "",
                "oem": {
                    "Port": 0,
                    "SupportedProtocol": ""
                }
            }
        ]
    }


def get_ethernet_interface_instance(wildcards):
    """
    Instantiate ethernet interface template
    """
    config = deepcopy(_TEMPLATE)
    config['@odata.context'] = config['@odata.context'].format(**wildcards)
    config['@odata.id'] = config['@odata.id'].format(**wildcards)
    config['Id'] = config['Id'].format(**wildcards)
    return config
