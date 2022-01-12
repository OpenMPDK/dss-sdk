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


# redfish_resource.py

# import pprint


class system(object):
    def __init__(self, uuid=""):
        self.uuid = uuid
        self.subsystems = list()    # class subsystem
        # self.ipv4 = list()
        # self.ipv6 = list()
        # self.ipv6gateway = ""

    def pprint(self):
        print("System:")
        print("  uuid=", self.uuid)
        for subsys in self.subsystems:
            subsys.pprint()


class subsystem(object):
    def __init__(self, uuid=""):
        self.name = ""
        self.uuid = uuid
        self.nqn = ""
        self.numa_aligned = False
        self.state = "Enabled"
        self.capacity = 0
        self.utilization = 0
        self.percent_avail = 0
        self.storage = list()       # class storage
        self.interfaces = list()    # class interface
        self.servername = ""
        self.nsid = 0

    def pprint(self):
        print("  Subsystem:")
        print("    name=", self.name)
        print("    uuid=", self.uuid)
        print("    nqn=", self.nqn)
        print("    numa_aligned=", self.numa_aligned)
        print("    state=", self.state)
        print("    capacity=", self.capacity)
        print("    utilization=", self.utilization)
        print("    servername=", self.servername)
        print("    nsid=", self.nsid)
        for intf in self.interfaces:
            intf.pprint()

        for stor in self.storage:
            stor.pprint()


class interface(object):
    def __init__(self):
        self.mac = ""
        self.name = ""
        self.port = 0
        self.speed = 0
        self.type = ""
        self.status = ""
        self.ip4_intf = list()   # class ipv4
        self.ip6_intf = list()   # class ipv6

    def pprint(self):
        print("    Interface:")
        print("      mac=", self.mac)
        print("      name=", self.name)
        print("      port=", self.port)
        print("      speed=", self.speed)
        print("      type=", self.type)
        print("      status=", self.status)
        for ip4 in self.ip4_intf:
            ip4.pprint()

        for ip6 in self.ip6_intf:
            ip6.pprint()


class ipv4(object):
    def __init__(self):
        self.addr = ""
        self.mask = ""
        self.origin = ""
        self.gateway = ""

    def pprint(self):
        print("      IPv4:")
        print("        addr=", self.addr)
        print("        mask=", self.mask)
        print("        origin=", self.origin)
        print("        gateway=", self.gateway)


class ipv6(object):
    def __init__(self):
        self.addr = ""
        self.plength = 0
        self.origin = ""
        self.state = "Disabled"
        self.gateway = ""

    def pprint(self):
        print("      IPv6:")
        print("        addr=", self.addr)
        print("        plength=", self.plength)
        print("        origin=", self.origin)
        print("        state=", self.state)
        print("        gateway=", self.gateway)


class storage(object):
    def __init__(self):
        self.uuid = ""
        self.capacity = 0          # total size of all drives
        self.utilization = 0       # total utilization of all drives
        self.percent_avail = 0
        self.drives = list()       # class drive

    def pprint(self):
        print("    Storage:")
        print("      uuid=", self.uuid)
        print("      capacity=", self.capacity)
        print("      utilization=", self.utilization)
        print("      percent_avail=", self.percent_avail)

        for drive in self.drives:
            drive.pprint()


class drive(object):
    def __init__(self):
        self.uuid = ""
        self.block_size = 512
        self.capacity = 0
        self.utilization = 0
        self.percent_avail = 0
        self.manufacturer = ""
        self.type = ""
        self.model = ""
        self.protocol = ""
        self.revision = ""
        self.sn = ""

    def pprint(self):
        print("      Drive:")
        print("        uuid=", self.uuid)
        print("        bsize=", self.block_size)
        print("        capacity=", self.capacity)
        print("        utilization=", self.utilization)
        print("        percent_avail=", self.percent_avail)
        print("        manufacturer=", self.manufacturer)
        print("        type=", self.type)
        print("        model=", self.model)
        print("        protocol=", self.protocol)
        print("        revision=", self.revision)
        print("        sn=", self.sn)


class fabric(object):
    def __init__(self):
        self.id = "NVMEoF"
        self.switches = list()    # class switch
        for switch in self.switches:
            switch.pprint()

    def pprint(self):
        print("Fabric:")
        print("    id=", self.id)


class switch(object):
    def __init__(self, uuid):
        self.id = ""
        self.SerialNumber = ""
        self.UUID = uuid
        self.Model = ""

        self.ports = list()
        for port in self.ports:
            port.pprint()

        self.vlans = list()
        for vlan in self.vlans:
            vlan.pprint()

    def pprint(self):
        print("    Switch:")
        print("        id=", self.id)
        print("        SerialNumber=", self.id)
        print("        UUID=", self.id)
        print("        Model=", self.id)


class port(object):
    def __init__(self, port_id):
        self.id = port_id
        self.status = ""

    def pprint(self):
        print("        Port:")
        print("            id=", self.id)
        print("            status=", self.status)


class vlan(object):
    def __init__(self, vlan_id):
        self.id = vlan_id

    def pprint(self):
        print("        VLAN:")
        print("            id=", self.id)
