# redfish_resource.py

import pprint

class system(object):
    def __init__(self, uuid=""):
        #print("system init. uuid=", uuid)
        self.uuid = uuid
        self.subsystems = list()    #class subsystem
        #self.ipv4 = list()
        #self.ipv6 = list()
        #self.ipv6gateway = ""

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
        self.storage = list()       #class storage
        self.interfaces = list()    #class interface
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
        self.ip4_intf = list()   #class ipv4
        self.ip6_intf = list()   #class ipv6

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
        return


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
        return


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
        return


class storage(object):
    def __init__(self):
        self.uuid = ""
        self.capacity = 0          # total size of all drives
        self.utilization = 0       # total utilization of all drives
        self.percent_avail = 0
        self.drives = list()    #class drive

    def pprint(self):
        print("    Storage:")
        print("      uuid=", self.uuid)
        print("      capacity=", self.capacity)
        print("      utilization=", self.utilization)
        print("      percent_avail=", self.percent_avail)
        for drive in self.drives:
            drive.pprint()
        return


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
        return





