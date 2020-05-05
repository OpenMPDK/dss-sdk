import array
import ctypes
import fcntl

NVME_IOCTL_ADMIN_CMD = 0xC0484E41

class NvmeAdminCmd(ctypes.Structure):
    _fields_ = [('opcode', ctypes.c_ubyte),
                ('flags', ctypes.c_ubyte),
                ('rsvd1', ctypes.c_ushort),
                ('nsid', ctypes.c_uint),
                ('cdw2', ctypes.c_uint),
                ('cdw3', ctypes.c_uint),
                ('metadata',ctypes.c_ulonglong),
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
