"""
Usage: kv-cli.py cpu list [options] [--]

Options:
  -h --help          Show this screen.
  --version          Show version.
"""
import json

from server_info.server_attributes_aggregator import OSMServerCPU


def cpu_identify():
    """
    Display local CPU details
    :return:
    """
    print("Display local CPU details")
    try:
        info_obj = OSMServerCPU()
        cpu_metadata, cpu_info = info_obj.get_all_cpu()
        print(json.dumps(cpu_metadata, sort_keys=True, indent=4))
    except:
        pass


def main(args):
    if "cpu" in args:
        if "list" in args:
            cpu_identify()
