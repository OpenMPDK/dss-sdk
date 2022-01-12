# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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
