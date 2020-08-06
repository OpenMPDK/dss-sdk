# KV CLI and Daemon Setup

Install dependencies in PYTHON_DEPENDENCIES.txt
Optional: 'pyinstaller --onefile kv-cli.py' will create a binary in ./dist

Run 'python kv-cli.py daemon 105.128.20.85 2379 &' from target server

Note: Use 105.128.20.85 and 2379 for now

Run 'python kv-cli.py kv list' from any computer that can reach 105.128.20.85:2379

Add subsystem by 'python kv-cli.py kv add --device <device_file> --server <uuid> --nqn <NQN> --core <core_id> --trtype RDMA --traddr <listen_ip> --trsvcid <listen_port>'

Remove subsystem by 'python kv-cli.py kv remove --server <uuid> --nqn <NQN>'


# Udev Monitor Setup

Copy '99-nvme-hotplug.rules' in './udev_monitor/' to '/etc/udev/rules.d/'

Copy 'SIGUSR1_kvcli.sh' to '/usr/bin/'

The '/usr/bin' directory can be changed.

You will need to modify the '99-nvme-hotplug.rules' file to reflect the new script path.

'chmod +x /usr/bin/SIGUSR1_kvcli.sh'

Reload udev rules by executing 'udevadm control --reload-rules && udevadm trigger'

Set the r/w permissions of the file if desired
