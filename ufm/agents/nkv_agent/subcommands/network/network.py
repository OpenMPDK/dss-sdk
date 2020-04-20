"""
Usage: kv-cli.py network list [options] [--]

Options:
  -h --help          Show this screen.
  --version          Show version.
  --type=<category>  Filter information type.
"""
import json

from server_info.server_attributes_aggregator import OSMServerNetwork


def network_identify():
    """
    Displays list of network interfaces on the system it's executed on.
    :return: 
    """
    print("List all network interfaces")
    info_obj = OSMServerNetwork()
    print(json.dumps(info_obj.identify_networkinterfaces(), sort_keys=True, indent=4))


def main(args):
    if "network" in args:
        if "list" in args:
            network_identify()
