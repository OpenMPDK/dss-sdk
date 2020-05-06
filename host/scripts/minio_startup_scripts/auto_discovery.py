import os,sys
import subprocess
import time
from utility import exception, exec_cmd
import traceback

SYS_BLOCK_PATH      = "/sys/block"
NVME_ADDRESS_FILE   = "/device/address"
NVME_NUMA_NODE_FILE = "/device/numa_node"
NVME_SUBSYSTEM_NQN  = "/device/subsysnqn"
NVME_TRANSPORT      = "/device/transport"

NVME_CONNECT_TIME   = 1 # seconds


class AutoDiscovery:

    def __init__(self , cluster_map=[]):
        self.cluster_map = cluster_map
        self.nqn_ip_port_to_remote_nvme = {}
        self.get_nvme_mount_dir()
        self.remote_nkv_mount_info = []


    def nvme_discover(self, transporter=None, address=None, port=None):
        """
        Discover all the transporter for a nqn
        :param nqn: <string>
        :return: <list> list of "nqn:address:port" for each address and port
        """
        print("INFO: Running nvme discovery ... ")
        discover_map = []
        if transporter and address and port:
            nvme_discover_command = "nvme discover " + " -t " + transporter + " -a " + address + " -s " + port
            ret, result = exec_cmd(nvme_discover_command, True, True)

            if not ret:
                subsystem_nqn = ""
                address       = ""
                port          = ""
                for line in result.split("\n"):

                    if line.startswith("subnqn:"):
                        subsystem_nqn = (line.split(" ")[-1]).strip()
                    elif line.startswith("trsvcid:"):
                        port    = (line.split(" ")[-1]).strip()
                    elif line.startswith("traddr:"):
                        address = (line.split(" ")[-1]).strip()

                    if  subsystem_nqn and address and port:
                        # store data
                        nqn_address_port = subsystem_nqn + ":" + address + ":" + port
                        discover_map.append(nqn_address_port)
                        subsystem_nqn = ""
                        address       = ""
                        port          = ""
        return discover_map



    def nvme_connect(self, transport=None, nqn=None, address=None, port=None):
        """
        Connect to the specified nqn:address:port
        :param transport: <string>  Type of transport such as tcp,rdbc
        :param nqn: <string>
        :param address: <string>
        :param port: <string>
        :return: None
        """
        command = "nvme connect -t " + transport + " -n " +  nqn + " -a " +  address + " -s " + port
        ret, result = exec_cmd(command, True, True)
        if not ret:
            print("INFO: connection successful for nqn - {}".format(nqn))
        else:
            print("ERROR: connection failure for nqn {}\n\t Return Code:{}, Msg: \"{}\"".format(nqn, ret,result.strip()))


    def nvme_disconnect(self, nqn):
        """
        Disconnect specified nqn for a subsystem
        :param nqn: <string>
        :return: None
        """
        command  = "nvme disconnect -n " + nqn
        ret, result = exec_cmd(command, True, True)
        if not ret:
            print("INFO: disconnected successful for nqn - {}".format(nqn))
        else:
            print("ERROR: Not able to disconnect for the nqn - {}\n\t Return Code:{}, Msg: {}".format(nqn, ret, result.strip()))

    @exception
    def get_address_port(self, nvme_dir):
        """
        Read address and port form a nvme dir.
        :param nvme_dir: <string> nvme remote directory...
        :return: <string> return <address>:<port>
        """
        address_file = SYS_BLOCK_PATH + "/" + nvme_dir + NVME_ADDRESS_FILE
        with open(address_file, "r") as FH:
            line = FH.readline()  # read only first line "traddr=102.100.20.1,trsvcid=1024"
            # Add regular expression
            address, port = line.split(',')
            address       = address.split('=')[1].strip()
            port          = (port.split('=')[1]).strip()


        return address + ":" + port

    @exception
    def get_numa_node(self, nvme_dir):
        """
        Read the numa code from the "/device/numa_node" file.
        :param nvme_dir:
        :return:
        """
        numa_node_file = SYS_BLOCK_PATH + "/" + nvme_dir  + NVME_NUMA_NODE_FILE
        numa_node = -1
        with open(numa_node_file, "r") as FH:
            line = FH.readline()
            numa_node = line.strip()

        return numa_node

    @exception
    def get_subsystem_nqn(self, nvme_dir):
        subsystem_nqn = SYS_BLOCK_PATH + "/" + nvme_dir + NVME_SUBSYSTEM_NQN
        nqn = ""
        with open(subsystem_nqn, "r") as FH:
            line = FH.readline()
            nqn = line.strip()
        return nqn
    @exception
    def get_transport(self, nvme_dir):
        transport_path = SYS_BLOCK_PATH + "/" + nvme_dir + NVME_TRANSPORT
        transport = ""
        with open(transport_path, "r") as FH:
            line = FH.readline()
            transport = line.strip()

        return transport

    @exception
    def  get_nvme_mount_dir(self):
        """
        Process "/dev/nvme0n1" mount paths.
        :return:
        """
        # List all the directories under /sys/block
        dirs = os.listdir(SYS_BLOCK_PATH)
        for nvme_dir in dirs:
            # Add regular expression
            if nvme_dir.startswith("nvme"):
                if self.get_transport(nvme_dir) == "tcp":
                    # Adderss, port
                    address_port =  self.get_address_port(nvme_dir)
                    # subsystem nqn
                    nqn = self.get_subsystem_nqn(nvme_dir)
                    nqn_address_port = nqn + ":" + address_port

                    #if self.nqn_ip_port_to_remote_nvme and nqn_address_port not in self.nqn_ip_port_to_remote_nvme:
                    self.nqn_ip_port_to_remote_nvme[nqn_address_port] = nvme_dir

    @exception
    def get_remote_mount_paths(self):
        """
        Figure out the remote mount paths ...
        :return:<dict>  A mapping subsystem to remote path.
        """

        remote_nkv_mount_paths = []
        subsystem_to_io_paths = {}
        for subsystem in self.cluster_map:
            nqn = subsystem["subsystem_nqn"]
            remote_target_node = subsystem["target_server_name"]

            # Each transporter
            for transport in subsystem["subsystem_transport"]:
                address = transport["subsystem_address"]
                port    = str(transport["subsystem_port"])
                nqn_address_port = nqn + ":" + address + ":" + port

                # Check if that present in nqn_address_port hash map
                if nqn_address_port not in self.nqn_ip_port_to_remote_nvme:

                    # NVME discover test
                    #print( "Discover:{}".format(self.nvme_discover("tcp",address,port)))

                    # First connect nqn_address_port to remote mount path
                    self.nvme_connect("tcp", nqn, address, port)
                    time.sleep(NVME_CONNECT_TIME)  # Require to connect complete its task
                    # Update nqn_address_port_remote_nvme_dir mapping
                    self.get_nvme_mount_dir()

                if self.nqn_ip_port_to_remote_nvme.get(nqn_address_port, False):
                    numa_node = self.get_numa_node(self.nqn_ip_port_to_remote_nvme[nqn_address_port])
                    remote_nkv_mount = {"mount_point": "/dev/" + self.nqn_ip_port_to_remote_nvme[nqn_address_port],
                                        "remote_nqn_name": nqn,
                                        "remote_target_node_name": remote_target_node,
                                        "nqn_transport_address": address,
                                        "nqn_transport_port": port,
                                        "numa_node_attached": numa_node,
                                        "sriver_thread_core": 2
                                        }
                    remote_nkv_mount_paths.append(remote_nkv_mount.get("mount_point",""))
                    # Get only one path from subsystem.
                    if nqn not in subsystem_to_io_paths:
                        if transport.get("subsystem_interface_status", 0):
                            subsystem_to_io_paths[nqn] = [remote_nkv_mount.get("mount_point","")]
                    else:
                        subsystem_to_io_paths[nqn].append(remote_nkv_mount.get("mount_point",""))

                    self.remote_nkv_mount_info.append(remote_nkv_mount)
        return subsystem_to_io_paths


