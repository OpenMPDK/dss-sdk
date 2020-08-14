UFM Overivew
============

The UFM is typically installed on 3 different machines. Each machine runs a message broker and a database.
Only the master UFM will serve up the Redfish servver, and the other two UFM's are idle. The HaProxy is used to direct all the traffic to the master UFM node.

The most convenient way to install the UFM and all it's prerequisite such as database, message-broker, web-server and etc is to use Ansible.
The Ansible install scripts can be found in the root director of the project.
Before running the Ansible script the inventory files (hosts) need to be modified, with the list of nodes for UFM and which node should run HaProxy.

.. code-block:: bash

    [fm_hosts]
    #host1
    #host2
    #host3

    [targets]
    #host4
    #host5

    [ha_proxy]
    #host6

The Ansible script installs and starts the cluster database and generates the appropriate services files.

One all the nodes are up and running the UFM can be stopped, restarted & started with the systemctl command.

Note: At this point in time all the configurations are only read at UFM startup.

Requirements
^^^^^^^^^^^^

The following python packages (python3) must to be installed before a UFM install package can be generated
and the modules in requirements.txt file in the project.

The UFM package can be build on both Ubuntu 16.04 and CentOS.

 - epel-release
 - ruby-devel
 - dpkg
 - fpm
 - gem
 - Sphinx
 - rinohtype
 - etcd3
 - Flask
 - Flask-HTTPAuth
 - Flask-RESTful
 - flatten-dict
 - pyzmq
 - zmq
 - configparser
 - netifaces
 - gunicorn
 - pyhaproxy
 - pytest
 - requests
 - redfish-client
 - redfish
 - PyYAML
 - jsonpath-ng
 - pip

Configuration
^^^^^^^^^^^^^

All the basic setting for the UFM are in the ufm.yaml file. The ufm.yaml configuration file is only
read at startup and controls which modules will be started.

The main module, "ufm" will always loaded and the other modules will be started if they are enabled.

See example:

.. code-block:: yaml

    ---
    ufm:
        dbType: "etcd"
        dbIp: "127.0.0.1"
        messageQueuePort: 5510
        externalPublisher: 5509
        brokerPort: 6000
        brokerIpFromDb: False

    nkv:
        enable: True
        start: True
        messageQueuePort: 5512

    switch:
        -   sw_type: mellanox
            sw_ip: 172.22.0.109
            enable: False
            start: False
            usrname: ssgroot
            pwd: proximal
            messageQueuePort: 5515

        -   sw_type: mellanox
            sw_ip: 172.22.0.110
            enable: False
            start: False
            usrname: ssgroot
            pwd: proximal
            messageQueuePort: 5516


UFM source code
^^^^^^^^^^^^^^^

The project can be cloned from

*git clone httbs://xx.xxx.xxx.xxx.xxx.xxx/nkv-sdk.git*


Build Install Package
^^^^^^^^^^^^^^^^^^^^^

Run the shell script makeufmpackage.sh, in the fabricmanager directory.

.. code-block:: bash

    ./makeufmpackage.sh -h


    makeufmpackage.sh  v1.00.02.00

    usage:
        -p    Generate deb and rpm packages
        -r    Convert deb package to rpm package
        -J    Add Jenkins build number

        -h    Show this help


To build the install packages:

.. code-block:: bash

    ./makeufmpackage.sh -p -r -J 000


The build script will generate both a deb and a rpm package.

.. code-block:: bash

    ufm_1.0.000.458e2b3-1.deb
    ufm-1.0.000.458e2b3-1.noarch.rpm

