#!/bin/python

import array
import ctypes
import fcntl
import logging
import os
import struct
import sys
import json

NVME_IOCTL_ADMIN_CMD = 0xC0484E41


class NvmeAdminCmd(ctypes.Structure):
    _fields_ = [('opcode', ctypes.c_ubyte),
                ('flags', ctypes.c_ubyte),
                ('rsvd1', ctypes.c_ushort),
                ('nsid', ctypes.c_uint),
                ('cdw2', ctypes.c_uint),
                ('cdw3', ctypes.c_uint),
                ('metadata', ctypes.c_ulonglong),
                ('addr', ctypes.c_ulonglong),
                ('metadata_len', ctypes.c_uint),
                ('data_len', ctypes.c_uint),
                ('cdw10', ctypes.c_uint),
                ('cdw11', ctypes.c_uint),
                ('cdw12', ctypes.c_uint),
                ('cdw13', ctypes.c_uint),
                ('cdw14', ctypes.c_uint),
                ('cdw15', ctypes.c_uint),
                ('timeout_ms', ctypes.c_uint),
                ('result', ctypes.c_uint), ]


log_file = sys.argv[0].split('.')[0] + ".log"

kv_fw_versions = ['ETA51KBE']

logging.basicConfig(
    filename=log_file, level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s - %(pathname)s [%(lineno)d] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S')


def ioctl_nvme_admin_command(device_path, opcode, nsid, data_len, cdw10):
    req = NvmeAdminCmd()
    req.opcode = opcode
    req.nsid = nsid
    req.data_len = data_len
    data = array.array('h', [0] * req.data_len)
    req.addr = data.buffer_info()[0]
    req.cdw10 = cdw10

    with open(device_path) as dev:
        ret = fcntl.ioctl(dev.fileno(), NVME_IOCTL_ADMIN_CMD, req)
        if ret == 0:
            return data
    return None


def read_linux_file(path):
    buf = None
    try:
        with open(path) as f:
            buf = str((f.read().rstrip()))
    finally:
        return buf


def get_logical_block_size(dev):
    name = dev.split('/')[-1]
    path = '/sys/block/' + name + '/queue/logical_block_size'
    logical_blk_sz = read_linux_file(path)
    if logical_blk_sz is None:
        return -1
    else:
        return logical_blk_sz


def unpack_identify_ns_details(data, d, fw_ver):
    if "LogicalBlockSize" not in d:
        return

    block_size = int(d["LogicalBlockSize"])
    disk_capacity, disk_utilization = struct.unpack_from('QQ', data,
                                                         offset=8)
    d["DiskCapacityInBytes"] = disk_capacity * block_size
    if fw_ver in kv_fw_versions:
        d["DiskUtilizationPercentage"] = "%.2f" % (float(disk_utilization) / 100)
        d["DiskUtilizationInBytes"] = int(
            (float(d["DiskUtilizationPercentage"]) / 100) * int(d["DiskCapacityInBytes"]))
    else:
        d["DiskUtilizationInBytes"] = int(disk_utilization * block_size)
        d["DiskUtilizationPercentage"] = int((disk_utilization * 100) / disk_capacity)


def get_firmware_rev(dev):
    name = dev.split('/')[-1]
    path = '/sys/block/' + name + '/device/firmware_rev'
    firmware_rev = read_linux_file(path)
    if firmware_rev is None:
        return ""
    else:
        return firmware_rev


def get_identify_ns_details(device_path):
    d = dict()
    try:
        data = ioctl_nvme_admin_command(device_path, 0x06, 1, 4096, 0)
        d['LogicalBlockSize'] = get_logical_block_size(device_path)
        d['FirmwareVersion'] = get_firmware_rev(device_path)
        if data is not None:
            unpack_identify_ns_details(data, d, d['FirmwareVersion'])
    except:
        logging.exception('Error in getting disk namespace details')

    return d


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Need to provide device name")
        print("Usage: python " + sys.argv[0] + " /dev/nvmeXXn1")
        sys.exit(-1)

    out = list()
    for dev_name in sys.argv[1:]:
        if not os.access(dev_name, os.O_RDONLY):
            logging.error("Device name " + dev_name + " doesn't exist")
            continue
        disk_out = get_identify_ns_details(dev_name)
        if disk_out:
            out.append({dev_name: disk_out})

    json_string = json.dumps(out)

    print(json_string)
