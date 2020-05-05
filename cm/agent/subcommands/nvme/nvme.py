"""
Usage: kv-cli.py nvme list [options] [--]

Options:
  -h --help          Show this screen.
  --version          Show version.
"""
import json

from server_info.server_attributes_aggregator import OSMServerStorage


def nvme_identify():
    """
    Displays list of NVMe devices on the system it's executed on.
    :return: 
    """
    print("List all NVMe devices")
    info_obj = OSMServerStorage()
    print(json.dumps(info_obj.get_storage_devices(info_obj.NVME), sort_keys=True, indent=4))


def main(args):
    if "nvme" in args:
        if "list" in args:
            nvme_identify()
