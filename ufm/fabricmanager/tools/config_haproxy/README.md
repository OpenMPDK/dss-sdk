# config_haproxy.py

This is a python library that lets the user update fields in the haproxy config
file. This supports the Python version 3.8.0

# Installation
This project needs the Pyhaproxy library to be installed.
Use 'pip3 install pyhaproxy' to install it

# Additional Requirements
A sample 'haproxy.cfg' file is first required for this library to make the
changes to. Only the fields available in the sample file can be updated.

# Usage
config_haproxy.py [-h] --addservers SERVER [--outdir FILEPATH]
optional arguments:
  -h, --help     show the help message and exit
  --addservers   comma separated list of servers in the format <server name>:<IP addr>:<Port>
  --outdir       path to the output file

# Example:
python3 config_haproxy.py --addservers server1:192.168.1.50:9000,server2:192.168.2.50:9000 --outdir /etc
