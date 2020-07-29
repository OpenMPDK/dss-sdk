UFM Quick Start Guide!
======================

Overview
^^^^^^^^

The Unified Fabric Manager (UFM) is a software to manage NKV's, ESSD's and Switches. The UFM exposed a RedFish API for all the types of the sub systems.

The typical UFM installation is with 3 nodes, where a UFM runs on each node. Only one of the UFM's is the master at the time, where the remaining UFM will be in idle mode.
All the UFM's are storing retrieving data from a distributed database (ETCD).


Also see the Getting Started With Simple Projects documentation, and for the best possible start, the UFM books. There is even a UFM Windows port to allow experimentation with UFM on a Windows host, using free tools, and without any special hardware requirements. New developers are also encouraged to make use of the configASSERT() macro.

Build quick start guide
^^^^^^^^^^^^^^^^^^^^^^^

UFM has been ported to many different architectures and compilers. Each UFM port is accompanied by a pre-configured demo application to get you up and running quickly. Better still, each demo application is accompanied by a documentation page providing full information on locating the UFM demo project source code, building the demo project, and configuring the target hardware.
The demo application documentation page also provided essential UFM port specific information, including how to write UFM compatible interrupt service routines, which is necessarily slightly different on different microcontroller architectures.

Follow these easy instructions to get up an running in minutes:

Guide
^^^^^

.. toctree::
   :maxdepth: 2

   ufm_config
   license.md

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
