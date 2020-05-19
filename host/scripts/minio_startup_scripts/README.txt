Requirements:
Python v3 and above.

Install dependent python modules
python3.6 -m pip install -r requirements.txt

Update configuration file ( config.json ):
- Update NKV library path
- Update NKV configuration file path
- Update Minio binary path

Run the script:
- Lunch Minio in non distrinuted mode:
  python3.6  minio.py -c config.json -cm

- Launch Minio in stand alone mode
  python3.6  minio.py -c config.json
- Launch two instances of Minio from a single host. By default uses first NIC from subsystem.
  python3.6  minio.py -c config.json -mi 2

    %>python3.6 minio.py -c config.json -t tcp -mh msl-ssg-mp02  -mi 2
    INFO: processing config file <path>/config.json
    INFO: Launching Minio with following remote mount paths ...
                                 - /dev/nvme0n1
                                 - /dev/nvme2n1
                                 - /dev/nvme4n1
                                 - /dev/nvme6n1
    INFO: Execution Cmd - minio  server  --address <IP address>:9000 /dev/nvme0n1 /dev/nvme2n1 /dev/nvme4n1 /dev/nvme6n1
    INFO: Minio instance - 1 is up
    INFO: Launching Minio with following remote mount paths ...
                                 - /dev/nvme0n1
                                 - /dev/nvme2n1
                                 - /dev/nvme4n1
                                 - /dev/nvme6n1
    INFO: Execution Cmd - minio  server  --address <IP address>:9001 /dev/nvme0n1 /dev/nvme2n1 /dev/nvme4n1 /dev/nvme6n1
    INFO: Minio instance - 2 is up

- Launch two instances of Minio from a single host with choice of NIC. Considering
  subsystem has multiple NIC. NIC index starts from 0.
  python3.6  minio.py -c config.json -mi 2 --nic_index 0
- Launch two instances of Minio from a single host for different NIC. Considering
  subsystem has multiple NIC. NIC index starts from 0.
  python3.6  minio.py -c config.json -mi 2 --different_nic

    %>python3.6 minio.py -c config.json -t tcp -mh msl-ssg-mp02  -mi 2  --different_nic
    INFO: processing config file <path>/config.json
    INFO: Launching Minio with following remote mount paths ...
                                 - /dev/nvme0n1
                                 - /dev/nvme2n1
                                 - /dev/nvme4n1
                                 - /dev/nvme6n1
    INFO: Execution Cmd - minio  server  --address <IP address>:9000 /dev/nvme0n1 /dev/nvme2n1 /dev/nvme4n1 /dev/nvme6n1
    INFO: Minio instance - 1 is up
    INFO: Launching Minio with following remote mount paths ...
                                 - /dev/nvme1n1
                                 - /dev/nvme3n1
                                 - /dev/nvme5n1
                                 - /dev/nvme7n1
    INFO: Execution Cmd - minio  server  --address <IP address>:9001 /dev/nvme1n1 /dev/nvme3n1 /dev/nvme5n1 /dev/nvme7n1
    INFO: Minio instance - 2 is up


- Lunch distributed Minio:
  python3.6  minio.py -c config.json --minio_distributed

  ** Make sure "/etc/hosts" file is updated as below. The host ip specification
  should start and end with #START/#END  keys.
  #START
  <IP address 1> minio1
  <IP address 2> minio2
  #END

     python3.6 minio.py -c config.json -t tcp -mh <machine name>  -mi 2  -dist
     INFO: processing config file <path>/config.json

     INFO: Launching Distributed Minio with following remote mount paths ...
                                 -  http://minio1/dev/nvme0n1
                                 -  http://minio1/dev/nvme2n1
                                 -  http://minio1/dev/nvme4n1
                                 -  http://minio1/dev/nvme6n1
                                 -  http://minio2/dev/nvme0n1
                                 -  http://minio2/dev/nvme2n1
                                 -  http://minio2/dev/nvme4n1
                                 -  http://minio2/dev/nvme6n1
     INFO: Execution Cmd - minio  server  --address <IP address>:9000  http://minio1/dev/nvme0n1  http://minio1/dev/nvme2n1  http://minio1/dev/nvme4n1  http://minio1/dev/nvme6n1  http://minio2/dev/nvme0n1  http://minio2/dev/nvme2n1  http://minio2/dev/nvme4n1  http://minio2/dev/nvme6n1
     INFO: Minio instance - 1 is up

     INFO: Launching Distributed Minio with following remote mount paths ...
                                 -  http://minio1/dev/nvme0n1
                                 -  http://minio1/dev/nvme2n1
                                 -  http://minio1/dev/nvme4n1
                                 -  http://minio1/dev/nvme6n1
                                 -  http://minio2/dev/nvme0n1
                                 -  http://minio2/dev/nvme2n1
                                 -  http://minio2/dev/nvme4n1
                                 -  http://minio2/dev/nvme6n1
     INFO: Execution Cmd - minio  server  --address <IP address>:9001  http://minio1/dev/nvme0n1  http://minio1/dev/nvme2n1  http://minio1/dev/nvme4n1  http://minio1/dev/nvme6n1  http://minio2/dev/nvme0n1  http://minio2/dev/nvme2n1  http://minio2/dev/nvme4n1  http://minio2/dev/nvme6n1
     INFO: Minio instance - 2 is up



- Dump ClusterMap:
  Add -cm switch to dump cluster map.
