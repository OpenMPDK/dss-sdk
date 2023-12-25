#!/usr/bin/python3

"""
# The Clear BSD License
#
# Copyright (c) 2023 Samsung Electronics Co., Ltd.
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
"""

from faker import Faker
import pytest
import secrets


default_mock_ip = '1.x.x.x'


@pytest.fixture(scope="session")
def get_sample_nvme_devices():
    sample_nvme_list_output = """\
        Node             SN                   Model                           \
                              Namespace Usage                      Format     \
                                            FW Rev
    ---------------- -------------------- ------------------------------------\
        ---- --------- -------------------------- ---------------- --------
    /dev/nvme0n1     DSS5066419069        SPDK bdev Controller                \
                  1          13.63  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme1n1     DSS2690548027        SPDK bdev Controller                \
                  1          13.63  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme2n1     DSS1137172735        SPDK bdev Controller                \
                  1          13.63  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme3n1     DSS7754076932        SPDK bdev Controller                \
                  1          12.58  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme4n1     DSS5066419069        SPDK bdev Controller                \
                  1          13.63  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme5n1     DSS2690548027        SPDK bdev Controller                \
                  1          13.63  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme6n1     DSS1137172735        SPDK bdev Controller                \
                  1          13.63  MB /   7.65  TB    512   B +  0 B   20.07
    /dev/nvme7n1     DSS7754076932        SPDK bdev Controller                \
                  1          12.58  MB /   7.65  TB    512   B +  0 B   20.07
    """
    return sample_nvme_list_output


@pytest.fixture(scope="session")
def get_sample_nvme_subsystems():
    sample_nvme_subsystems = """\
    nvme-subsys0 - NQN=nqn.2023-06.io:msl-ssg-sm1-kv_data1
    \
    +- nvme0 rdma traddr=201.x.x.xxx trsvcid=1024 live
    +- nvme4 rdma traddr=203.x.x.xxx trsvcid=1024 live
    nvme-subsys1 - NQN=nqn.2023-06.io:msl-ssg-sm1-kv_data2
    \
    +- nvme1 rdma traddr=201.x.x.xxx trsvcid=1024 live
    +- nvme5 rdma traddr=203.x.x.xxx trsvcid=1024 live
    nvme-subsys2 - NQN=nqn.2023-06.io:msl-ssg-sm1-kv_data3
    \
    +- nvme2 rdma traddr=201.x.x.xxx trsvcid=1024 live
    +- nvme6 rdma traddr=203.x.x.xxx trsvcid=1024 live
    nvme-subsys3 - NQN=nqn.2023-06.io:msl-ssg-sm1-kv_data4
    \
    +- nvme3 rdma traddr=201.x.x.xxx trsvcid=1024 live
    +- nvme7 rdma traddr=203.x.x.xxx trsvcid=1024 live
    """
    return sample_nvme_subsystems


def get_sample_lscpu_output():
    sample_lscpu_output = """\
    Architecture:          x86_64
    CPU op-mode(s):        32-bit, 64-bit
    Byte Order:            Little Endian
    CPU(s):                88
    On-line CPU(s) list:   0-87
    Thread(s) per core:    2
    Core(s) per socket:    22
    Socket(s):             2
    NUMA node(s):          2
    Vendor ID:             GenuineIntel
    CPU family:            6
    Model:                 85
    Model name:            Intel(R) Xeon(R) Gold 6152 CPU @ 2.10GHz
    Stepping:              4
    CPU MHz:               2543.753
    CPU max MHz:           3700.0000
    CPU min MHz:           1000.0000
    BogoMIPS:              4200.00
    Virtualization:        VT-x
    L1d cache:             32K
    L1i cache:             32K
    L2 cache:              1024K
    L3 cache:              30976K
    NUMA node0 CPU(s):     0-21,44-65
    NUMA node1 CPU(s):     22-43,66-87
    Flags:                 fpu vme de pse tsc msr pae mce cx8 apic sep mtrr\
        mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe\
            syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs\
                bts rep_good nopl xtopology nonstop_tsc cpuid aperfmperf pni\
                    pclmulqdq dtes64 monitor ds_cpl vmx smx tm2 ssse3 sdbg\
                        fma cx16 xtpr pdcm pcid dca sse4_1 x2apic movbe\
                            popcnt tsc_deadline_timer aes xsave avx rdrand\
                                lahf_lm abm 3dnowprefetch cpuid_fault epb\
                                    cat_l3 cdp_l3 pti intel_ppin\
                                        ssbd mba ibrs ibpb stibp\
                                            vnmi ept vpid ept_ad\
                                                fsgsbase tsc_adjust bmi1 hle\
                                                    avx2 smep bmi2 erms\
                                                        rtm cqm mpx rdt_a\
    """
    return sample_lscpu_output


@pytest.fixture(scope="session")
def get_sample_nvme_discover_output():
    sample_nvme_dsicover_output = """\
    Discovery Log Number of Records 1, Generation counter 4
    =====Discovery Log Entry 0======
    trtype:  rdma
    adrfam:  ipv4
    subtype: nvme subsystem
    treq:    not required
    portid:  0
    trsvcid: 1024
    subnqn:  nqn.2023-06.io:msl-dpe-perf35-kv_data1
    traddr:  x.x.x.x
    rdma_prtype: not specified
    rdma_qptype: connected
    rdma_cms:    rdma-cm
    rdma_pkey: 0x0000
    """
    return sample_nvme_dsicover_output


@pytest.fixture(scope="session")
def get_sample_dict_data():
    sample_dict = {"key " + str(i): secrets.randbelow(100) for i in range(10)}
    return sample_dict


@pytest.fixture(scope="session")
def get_temp_dir(tmpdir):
    return tmpdir


@pytest.fixture
def get_sample_ipv4_addr():
    fake = Faker()
    return fake.ipv4()


@pytest.fixture
def get_sample_ipv6_addr():
    fake = Faker()
    return fake.ipv6()


@pytest.fixture
def get_sample_ipv4_addr_list():
    fake = Faker()
    return [fake.ipv4() for _ in range(4)]


@pytest.fixture
def get_sample_ipv6_addr_list():
    fake = Faker()
    return [fake.ipv6() for _ in range(4)]


@pytest.fixture
def get_global_var():
    global rdd
    rdd = "yes"
    return rdd


def get_sample_lshw_output():
    return """\
        {
        "id" : "network:0",
        "class" : "network",
        "claimed" : true,
        "handle" : "PCI:0000:04:00.0",
        "description" : "Ethernet interface",
        "product" : "MT27800 Family [ConnectX-5]",
        "vendor" : "Mellanox Technologies",
        "physid" : "0",
        "businfo" : "pci@0000:04:00.0",
        "logicalname" : "p6p1",
        "version" : "00",
        "serial" : "b8:59:9f:2d:18:7e",
        "width" : 64,
        "clock" : 33000000,
        "configuration" : {
            "autonegotiation" : "on",
            "broadcast" : "yes",
            "driver" : "mlx5_core",
            "driverversion" : "5.0-0",
            "firmware" : "16.33.1048 (MT_0000000012)",
            "latency" : "0",
            "link" : "no",
            "multicast" : "yes",
            "port" : "fibre",
            "ip": "192.168.1.x"
        },
        "capabilities" : {
            "pciexpress" : "PCI Express",
            "vpd" : "Vital Product Data",
            "msix" : "MSI-X",
            "pm" : "Power Management",
            "bus_master" : "bus mastering",
            "cap_list" : "PCI capabilities listing",
            "rom" : "extension ROM",
            "ethernet" : true,
            "physical" : "Physical interface",
            "fibre" : "optical fibre",
            "autonegotiation" : "Auto-negotiation"
        }
        }
        """


def get_sample_nvme_list_subsys():
    return """\
    {
  "Subsystems" : [
    {
      "Name" : "nvme-subsys0",
      "NQN" : "nqn.2023-12.io:109-tcp-0-kv_data1"
    },
    {
      "Paths" : [
        {
          "Name" : "nvme0",
          "Transport" : "rdma",
          "Address" : "traddr=192.168.1.x trsvcid=1024",
          "State" : "live"
        }
      ]
    },
    {
      "Name" : "nvme-subsys1",
      "NQN" : "nqn.2023-12.io:109-tcp-0-kv_data2"
    },
    {
      "Paths" : [
        {
          "Name" : "nvme1",
          "Transport" : "rdma",
          "Address" : "traddr=192.168.1.x trsvcid=1024",
          "State" : "live"
        }
      ]
    },
    {
      "Name" : "nvme-subsys2",
      "NQN" : "nqn.2023-12.io:109-tcp-0-kv_data3"
    },
    {
      "Paths" : [
        {
          "Name" : "nvme2",
          "Transport" : "rdma",
          "Address" : "traddr=192.168.1.x trsvcid=1024",
          "State" : "live"
        }
      ]
    },
    {
      "Name" : "nvme-subsys3",
      "NQN" : "nqn.2023-12.io:109-tcp-0-kv_data4"
    },
    {
      "Paths" : [
        {
          "Name" : "nvme3",
          "Transport" : "rdma",
          "Address" : "traddr=192.168.1.x trsvcid=1024",
          "State" : "live"
        }
      ]
    }
  ]
}
        """


def get_ls_pci_bus(cmd):
    begin = "/sys/class/pci_bus/0000:17/" +\
            "device/0000:17:00.0/nvme/nvme1/"
    if cmd.endswith("firmware_rev"):
        return begin + "firmware_rev"
    else:
        return begin + "serial"


def get_cat_sys_class_net(cmd):
    if cmd.endswith("mtu"):
        return "9000"
    else:
        return "0"


def mock_exec_cmd(cmd):
    ls_pci_bus = "ls /sys/class/pci_bus/"
    cat_sys_class_net = "cat /sys/class/net/"
    nvme_list_subsys = "nvme list-subsys -o json"
    lspci_mm_n_D = "lspci -mm -n -D | grep 0108 "
    lshw = "lshw -c network -json"
    ip_d = "ip -d link show dev"
    res = ""

    if cmd == "uname -r":
        res = "5.1.0"
    elif cmd.startswith(nvme_list_subsys):
        res = get_sample_nvme_list_subsys()
    elif cmd.startswith("ip -4 addr | grep"):
        res = "192.168.1.x/24"
    elif cmd.startswith(ls_pci_bus):
        res = get_ls_pci_bus(cmd)
    elif cmd.startswith(lspci_mm_n_D):
        res = "0000:17:00.0"
    elif cmd.startswith(cat_sys_class_net):
        res = get_cat_sys_class_net(cmd)
    elif cmd == "lscpu":
        res = get_sample_lscpu_output()
    elif cmd == "nproc":
        res = "128"
    elif cmd == lshw:
        res = get_sample_lshw_output()
    elif cmd.startswith(ip_d):
        res = "vlanid 1234"

    return 0, res, ""


def mock_os_dirname_host(name):
    return "/usr/dss/nkv-sdk/bin"


def mock_exec_cmd_remote(cmd, host, user, pw):
    if cmd.startswith("lshw -c network -json"):
        return 0, get_sample_lshw_output(), ""
    elif cmd.startswith("ip -d link show dev"):
        return 0, ["vlanid 1234"], ""
    else:
        return 0, "", ""


def mock_interfaces():
    return ['nic']


def mock_ifaddresses(interface):
    return [[{'addr': default_mock_ip}]]


class MockArgs():

    def __init__(self, *args, **kwargs):
        self.addrs = [default_mock_ip]
        self.ports = [1234]
        self.command = "config_host"
        self.gen2 = False
        self.vlan_ip_map = None
        self.rdd_port = None
        self.proto = "rdma"
        self.memalign = 512
        self.qpair = 32
        self.kvpair = []


class MockArgParser():

    def __init__(self, *args, **kwargs):
        # mock function, do nothing
        pass

    def parse_args(self, *args, **kwargs):
        return MockArgs()

    def add_argument(self, *args, **kwargs):
        # mock function, do nothing
        pass
