###dss_host.py 
Premable:
dss_host handles 2 main tasks:
    A. Configure host to discover and connect to nvme subsystems
    B. Use nvme subsystem information to generate minio run scripts


A: Configure host to discover and connect to nvme subsystems
    Option 1: Configure using autodiscovery based on VLAN ID and host/port list
    python dss_host.py config_host --vlan-ids space-separated list of vlan ids -t <rdma/tcp> --hosts space-separated list of hostnames --ports space-separated list of ports to scan
        This will find the IPs of all of the interfaces on the specified VLANs on all of the hosts.
        It will then attempt to connect to an nvme subsystem every combination of IP:port using IPs from the previous step and ports from the command line argument --ports
        If it fails to connect to an nvme subsystem, it will exit without cleaning up. Cleanup can be done by running python dss_host.py remove

    Option 2: Configure using explicit address list
    python dss_host.py config_host --addrs ip0:port0 ip1:port1 ip2:port2 ... -t <rdma/tcp>
        Same as option 1, except no automatic discovery of IP/port

    Command line flags:
        Option 1: ALL of --vlan-ids --ports --hosts must be specified, but not --addrs
            -vids/--vlan-ids: space delimited list of (numeric) vlan ids
            -p/--ports: space delimited list of ports to search for nvme-discover
            --hosts: list of hostnames (can be name or IP) to search
        Option 2: --addrs must be specified, but --vlan-ids, --ports, --hosts must not
            -a/--addrs: List of IP:PORT, cannot be specified with -vids/-p/--hosts
        Common:
        -i/--qpair: # queue pairs for rdma
        -m/--memalign: memory alignment for driver (e.g. 512 for 512B alignment)


B: Generate minio run scripts
    Option 1: Configure distributed minio automatically using nvme-subsys
        python dss_host.py config_minio --port PORT
            

    Option 2: Configure minio manually (dist or sa)

Both options will automatically create a run script for every path in each subsystem
The script has the naming convention 'minio_startup_<IP>.sh where <IP> is the numeric ipv4 address of the server
In addition to the run scripts, a file called etc_hosts is generated, containing host entries for the targets. The contents of etc_hosts must be added to /etc/hosts on every server running minio in the cluster

config_minio should only be run on 1 server, and the output files should be distributed to the other servers in the cluster.

