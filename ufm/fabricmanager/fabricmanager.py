# External Imports
import os
import os.path
import sys
import json
import traceback
import socket
import signal
import time
import argparse
import uuid
import threading
from multiprocessing import Process

import yaml

# Flask Imports
from flask import Flask, make_response, render_template
from flask_restful import Api, Resource
# from flask_restful import reqparse
# from flask import request

# REST API Imports
from rest_api.redfish.JsonSchemas import JsonSchemas
from rest_api.resource_manager import ResourceManager
# from rest_api.redfish.System_api import UfmdbSystemAPI

# Internal Imports
import config
from common.ufmlog import ufmlog
from common.ufmdb import ufmdb

# Depricated library
from common.clusterlib import lib

from ufm_status import UfmStatus
from ufm_status import UfmLeaderStatus
from ufm_status import UfmHealthStatus

# Redfish
from rest_api.redfish import redfish_constants
from rest_api.redfish.ServiceRoot import ServiceRoot
from rest_api.redfish.System_api import SystemCollectionEmulationAPI, \
    SystemEmulationAPI, \
    SystemAPI, \
    SystemCollectionAPI
from rest_api.redfish.Storage import StorageCollectionEmulationAPI, \
    StorageEmulationAPI, \
    StorageCollectionAPI, \
    StorageAPI
from rest_api.redfish.Drive import DriveCollectionEmulationAPI, \
    DriveEmulationAPI, DriveCollectionAPI, DriveAPI
from rest_api.redfish.EthernetInterface import \
    EthernetInterfaceCollectionEmulationAPI, \
    EthernetInterfaceEmulationAPI, \
    EthernetInterfaceAPI, \
    EthernetInterfaceCollectionAPI
from rest_api.redfish.Fabric_api import FabricCollectionEmulationAPI, \
    FabricEmulationAPI, \
    FabricAPI, \
    FabricCollectionAPI
from rest_api.redfish.Switch import SwitchEmulationAPI, \
    SwitchCollectionEmulationAPI, \
    SwitchCollectionAPI, \
    SwitchAPI, \
    SwitchActionAPI
from rest_api.redfish.Port import PortCollectionEmulationAPI, \
    PortEmulationAPI, \
    PortCollectionAPI, \
    PortAPI, \
    PortActionAPI
from rest_api.redfish.VLAN import VlanCollectionEmulationAPI, \
    VlanEmulationAPI, \
    VlanCollectionAPI, \
    VlanCollectionActionAPI, \
    VlanAPI, \
    VlanActionAPI

from backend.populate import populate

from systems.ufm_message import Publisher

from systems.ufm.ufm import Ufm
from systems.ufmarg import UfmArg

from systems.switch.switch import EthSwitch
from systems.switch.switch_arg import SwitchArg

from systems.essd import essd_constants

from systems.nkv.nkv import Nkv


# If application is installed, then append the module search path
global ufmlog
if os.path.dirname(os.path.realpath(__file__)) == "/usr/share/ufm":
    sys.path.append("/usr/share/ufm/")
    # This log path is created by the installer

    ufmlog.ufmlog.filename = "/var/log/ufm/ufm.log"
else:
    ufmlog.ufmlog.filename = "ufm.log"

log = ufmlog.log(module="main", mask=ufmlog.UFM_MAIN)

# Global variables to initialize resource manager
CONFIG_LOGFILE = '/tmp/fabricmanager-log.conf'

LOCAL_HOST = "127.0.0.1"

# Base URL of REST APIs
REST_BASE = redfish_constants.REST_BASE + '/'

# Create Flask server
app = Flask(__name__)

# Disable strict trailing slash for all urls
app.url_map.strict_slashes = False

# Create RESTful API
api = Api(app)

connect_to_single_db_node = False

# Time between each check of which FM is the lead
MASTER_CHECK_INTERVAL = 1

CLUSTER_TIMEOUT = (3 * 60)

ufmMainEvent = threading.Event()

# MODE: static, local or db
# MODE determines the data source for REST API resources
MODE = 'db'
local_path = 'backend/local-config.json'

# Server's arguments
kwargs = {}


def signal_handler(sig, frame):
    ufmMainEvent.set()


# End point defined for http://<host:port>/
@app.route("/")
def index():
    return render_template('index.html')


@api.representation('application/json')
def output_json(data, code, headers=None):
    """
    Overriding how JSON is returned by the server so that it looks nice
    """
    response = make_response(json.dumps(data, indent=4), code)
    response.headers.extend(headers or {})
    return response


def error_response(msg, status, jsonify=False):
    data = {
        'Status': status,
        'Message': '{}'.format(msg)
    }

    if jsonify:
        data = json.dumps(data, indent=4)

    return data, status


global resource_manager

# Industry Standard Software Defined Management for Converged, Hybrid IT,
# developed by DMTF group, that utilizes JSON schema or OData


class RedfishAPI(Resource):
    def __init__(self):
        super(RedfishAPI, self).__init__()

    """
    Return ServiceRoot
    """
    def get(self, path=None):
        try:
            if path is not None:
                config = RedfishAPI.load_configuration(resource_manager, path)
            else:
                config = resource_manager.configuration

            response = config, 200
        except Exception:
            traceback.print_exc()
            response = error_response('Internal Server Error', 500)

        return response

    # For any path, other than rest base
    @staticmethod
    def load_configuration(resource_manager, path):
        config = resource_manager.load_resource_dictionary(path)
        return config


def call_populate(local_path):
    """
    Construct dynamic resources using local data at backend/local-config.json
    """
    try:
        if os.path.exists(local_path) and os.path.isfile(local_path):
            with open(local_path, 'r') as f:
                local_config = json.load(f)
                populate(local_config['LOCAL'])

    except Exception as e:
        print(e)
        print('ERR: Reading Local Config File')


# For any other RESTful requests, navigate them to RedfishAPI object
# Note: <path:path> specifies any path
if (MODE is not None and MODE.lower() == 'db'):
    api.add_resource(ServiceRoot, REST_BASE,
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(JsonSchemas, REST_BASE + 'JSONSchemas')
    api.add_resource(SystemCollectionAPI, REST_BASE + 'Systems')
    api.add_resource(SystemAPI, REST_BASE + 'Systems/<string:ident>')
    api.add_resource(EthernetInterfaceCollectionAPI, REST_BASE + 'Systems/<string:sys_id>/EthernetInterfaces')
    api.add_resource(EthernetInterfaceAPI, REST_BASE + 'Systems/<string:sys_id>/EthernetInterfaces/<string:eth_id>')
    api.add_resource(StorageCollectionAPI, REST_BASE + 'Systems/<string:sys_id>/Storage')
    api.add_resource(StorageAPI, REST_BASE + 'Systems/<string:sys_id>/Storage/<string:storage_id>')
    api.add_resource(DriveCollectionAPI, REST_BASE + 'Systems/<string:sys_id>/Storage/<string:storage_id>')
    api.add_resource(DriveAPI, REST_BASE + 'Systems/<string:sys_id>/Storage/<string:storage_id>/Drives/<string:drive_id>')
    # api.add_resource(UfmdbSystemAPI, '/<path:path>')

    api.add_resource(FabricAPI, REST_BASE + 'Fabrics/<string:fab_id>')
    api.add_resource(FabricCollectionAPI, REST_BASE + 'Fabrics')
    api.add_resource(SwitchAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>')
    api.add_resource(SwitchActionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/Actions/<string:act_str>')
    api.add_resource(SwitchCollectionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches')
    api.add_resource(PortAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/Ports/<string:port_id>')
    api.add_resource(PortActionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/Ports/<string:port_id>/Actions/<string:act_str>')
    api.add_resource(PortCollectionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/Ports')
    api.add_resource(VlanAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/VLANs/<string:vlan_id>')
    api.add_resource(VlanActionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/VLANs/<string:vlan_id>/Actions/<string:act_str>')
    api.add_resource(VlanCollectionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/VLANs')
    api.add_resource(VlanCollectionActionAPI, REST_BASE + 'Fabrics/<string:fab_id>/Switches/<string:sw_id>/VLANs/Actions/<string:act_str>')


elif (MODE is not None and MODE.lower() == 'local'):
    api.add_resource(ServiceRoot, REST_BASE,
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(SystemCollectionEmulationAPI, REST_BASE + 'Systems')
    api.add_resource(SystemEmulationAPI, REST_BASE + 'Systems/<string:ident>')
    api.add_resource(StorageCollectionEmulationAPI, REST_BASE + 'Systems/<string:ident>/Storage',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Systems'})
    api.add_resource(StorageEmulationAPI, REST_BASE + 'Systems/<string:ident1>/Storage/<string:ident2>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(DriveCollectionEmulationAPI, REST_BASE + 'Systems/<string:ident1>/Storage/<string:ident2>/Drives',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Systems'})
    api.add_resource(DriveEmulationAPI, REST_BASE + 'Systems/<string:ident1>/Storage/<string:ident2>/Drives/<string:ident3>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(EthernetInterfaceCollectionEmulationAPI, REST_BASE + 'Systems/<string:ident>/EthernetInterfaces',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Systems'})
    api.add_resource(EthernetInterfaceEmulationAPI, REST_BASE + 'Systems/<string:ident1>/EthernetInterfaces/<string:ident2>',
                     resource_class_kwargs={'rest_base': REST_BASE})

    api.add_resource(FabricCollectionEmulationAPI, REST_BASE + 'Fabrics')
    api.add_resource(FabricEmulationAPI, REST_BASE + 'Fabrics/<string:ident>')
    api.add_resource(SwitchCollectionEmulationAPI, REST_BASE + 'Fabrics/<string:ident>/Switches',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Fabrics'})
    api.add_resource(SwitchEmulationAPI, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(PortCollectionEmulationAPI, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/Ports',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Fabrics'})
    api.add_resource(PortEmulationAPI, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/Ports/<string:ident3>',
                     resource_class_kwargs={'rest_base': REST_BASE})
    api.add_resource(VlanCollectionEmulationAPI, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/VLANs',
                     resource_class_kwargs={'rest_base': REST_BASE, 'suffix': 'Fabrics'})
    api.add_resource(VlanEmulationAPI, REST_BASE + 'Fabrics/<string:ident1>/Switches/<string:ident2>/VLANs/<string:ident3>',
                     resource_class_kwargs={'rest_base': REST_BASE})

    call_populate(local_path)
else:
    api.add_resource(RedfishAPI, REST_BASE,
                     REST_BASE + '<path:path>')
    try:
        resource_manager = ResourceManager(REST_BASE, MODE)
    except Exception:
        log.error('Failed to Initialize Resource Manager')


def startSubSystems(sub_systems):
    """
    Start all sub_systems and pass in a ref to logger
    """
    for sub in sub_systems:
        if not sub.is_running():
            sub.start()


def stopSubSystems(sub_systems):
    for sub in reversed(sub_systems):
        sub.stop()


def insertEssdUrls(db, essdUrls):
    listOfDrives = list()
    try:
        tmpString, _ = db.get(essd_constants.ESSDURLS_KEY)

        if tmpString:
            listOfDrives = json.loads(tmpString.decode('utf-8'))
    except Exception:
        pass

    for d in essdUrls:
        listOfDrives.append(d)

    # remove duplicates from list
    res = []
    for i in listOfDrives:
        if i not in res:
            res.append(i)
    res.sort()

    jsonString = json.dumps(res, indent=4, sort_keys=True)
    db.put(essd_constants.ESSDURLS_KEY, jsonString)


def readConfigDataFromFile(filename):
    installPath = "/usr/share/ufm/"

    configData = dict()
    if os.path.isfile(installPath + filename):
        with open(installPath + filename) as f:
            configData = yaml.safe_load(f)
    else:
        if os.path.isfile(filename):
            with open(filename) as f:
                configData = yaml.safe_load(f)
        else:
            log.error("Failed to open config file ufm.yaml")
            sys.exit(-1)

    return configData


def parseUfmConfig(ufmArg=None, ufmMetadata=None):
    ufmArg.ufmPorts = list()
    try:
        ufmArg.ufmConfig = ufmMetadata['ufm']
        ufmArg.ufmPorts.append(ufmArg.ufmConfig['messageQueuePort'])
    except Exception:
        return False

    try:
        ufmArg.nkvConfig = ufmMetadata['nkv']
        ufmArg.ufmPorts.append(ufmArg.nkvConfig['messageQueuePort'])
    except Exception:
        ufmArg.nkvConfig['enable'] = False

    try:
        ufmArg.switchConfig = ufmMetadata['switch']
        # ufmArg.ufmPorts.append(ufmArg.switchConfig['messageQueuePort'])
    except Exception:
        ufmArg.switchConfig['enable'] = False

    return True


def initializeSubSystems(subSystems=None, ufmArg=None, ufmMetadata=None):
    # Ufm is required
    subSystems.append(Ufm(ufmArg))

    try:
        if ufmArg.nkvConfig['enable']:
            subSystems.append(
                Nkv(ufmArg=ufmArg,
                    hostname=ufmArg.hostname,
                    db=ufmArg.deprecatedDb)
            )
    except Exception as e:
        log.exception(e)
        pass

    try:
        for switch_arg in ufmArg.switchConfig:
            if switch_arg['enable']:
                swArg = SwitchArg(sw_type=switch_arg['sw_type'],
                                  sw_ip=switch_arg['sw_ip'],
                                  log=ufmArg.log,
                                  db=ufmArg.db,
                                  usrname=switch_arg['usrname'],
                                  pwd=switch_arg['pwd'],
                                  port=switch_arg['messageQueuePort'])
                subSystems.append(EthSwitch(swArg))
    except Exception as e:
        log.exception(e)
        pass


class StatusChangeCbArg(object):
    def __init__(self, subsystems, target, kwargs, ufmArg, log):
        self.subsystems = subsystems
        self.target = target
        self.kwargs = kwargs
        self.ufmArg = ufmArg
        self.apiServer = None
        self.isRunning = False
        self.log = log


def setMasterInDb(ufmArg):
    masterInfo = dict()
    masterInfo['hostname'] = ufmArg.hostname
    masterInfo['uuid'] = ufmArg.uuid
    masterInfo['starttime'] = time.time()

    jsonString = json.dumps(masterInfo, indent=4, sort_keys=True)
    ufmArg.db.put('/ufm/master', jsonString)


def serverStateChange(startup, cbArgs):
    if startup is True and cbArgs.isRunning is not True:
        cbArgs.log.info('serverStateChange: Starting up UFM')
        setMasterInDb(ufmArg=cbArgs.ufmArg)
        cbArgs.server = Process(target=cbArgs.target, kwargs=cbArgs.kwargs)
        cbArgs.server.start()
        startSubSystems(cbArgs.subsystems)
        cbArgs.isRunning = True
    elif startup is not True and cbArgs.isRunning is True:
        cbArgs.log.info('serverStateChange: Shutting down UFM')
        cbArgs.server.terminate()
        cbArgs.server.join()
        stopSubSystems(cbArgs.subsystems)
        cbArgs.isRunning = False


def onHealthChange(ufmHealthStatus, cbArgs):
    cbArgs.log.info(f'onHealthChange: {ufmHealthStatus}, isRunning: {cbArgs.isRunning}')
    if UfmHealthStatus.HEALTHY is not ufmHealthStatus:
        serverStateChange(False, cbArgs)


def onLeaderChange(ufmLeaderStatus, cbArgs):
    cbArgs.log.info(f'onLeaderChange: {ufmLeaderStatus}, isRunning: {cbArgs.isRunning}')
    if UfmLeaderStatus.LEADER is ufmLeaderStatus:
        serverStateChange(True, cbArgs)
    else:
        serverStateChange(False, cbArgs)


def main():
    """
    The master fabricmanager does all the work. The node that has the
    ETCD master(lead) also run the master fabricmanager.

    Note: The machines with the cluster must have names
    """
    log.log_detail_on()
    log.log_debug_on()
    log.info('===> FabricManager Startup <===')

    # To initialize REST API resources
    global CONFIG_LOGFILE
    global LOCAL_HOST
    global resource_manager

    log.detail('main: Setting up signal handler')
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGQUIT, signal_handler)

    log.detail('main: Parsing args')
    parser = argparse.ArgumentParser(
        description='Process Server\'s Configuration Settings.'
    )
    parser.add_argument("--host",
                        help="IP Address of Server",
                        dest="host",
                        default="0.0.0.0"
                        )
    parser.add_argument("--port",
                        help="Port of Server",
                        dest="port",
                        default=5001)
    parser.add_argument("--logger_config",
                        help="fabricmanager logger config file",
                        dest="config_logfile",
                        default=CONFIG_LOGFILE
                        )

    args = parser.parse_args()

    log.detail('config_logfile: %s', args.config_logfile)

    # Default fabricmanager-config.json data.
    #    Override with user input data if any
    kwargs['host'] = args.host
    kwargs['port'] = args.port

    ufmMetadata = readConfigDataFromFile(filename="ufm.yaml")

    ufmArg = UfmArg()
    if not parseUfmConfig(ufmArg, ufmMetadata):
        log.error("Failed to parse ufm config data.")
        sys.exit(-1)

    try:
        dbType = ufmArg.ufmConfig['dbType']
        dbAddress = ufmArg.ufmConfig['dbIp']
        ufmMessageQueuePort = ufmArg.ufmConfig['messageQueuePort']
    except Exception:
        log.error("Failed to read ufm configuration file.")
        sys.exit(-1)

    try:
        db = ufmdb.client(db_type=dbType)
    except Exception:
        log.error("Failed to connect to database.")
        sys.exit(-1)

    # Connect to db with deprecated library.
    try:
        deprecatedDb = lib.create_db_connection(ip_address=dbAddress, log=log)
    except Exception:
        log.error("Failed to connect to deprecated DB.")
        sys.exit(-1)

    config.rest_base = REST_BASE

    # Main UFM services are required
    hostname = socket.gethostname().lower()

    ufmArg.set(db=db,
               hostname=hostname,
               log=log,
               uuid=uuid.getnode(),
               ufmMainEvent=ufmMainEvent,
               publisher=Publisher(ufmMessageQueuePort))
    ufmArg.deprecatedDb = deprecatedDb

    subSystems = list()
    initializeSubSystems(
        subSystems=subSystems,
        ufmArg=ufmArg,
        ufmMetadata=ufmMetadata
    )

    # ufm_status will handle all health and leader checks
    #
    # UfmStatus monitors cluster for health and determines leader
    status_cb_arg = StatusChangeCbArg(subsystems=subSystems,
                                      target=app.run,
                                      kwargs=kwargs,
                                      ufmArg=ufmArg,
                                      log=log)

    ufm_status = UfmStatus(onHealthChangeCb=onHealthChange,
                           onHealthChangeCbArgs=status_cb_arg,
                           onLeaderChangeCb=onLeaderChange,
                           onLeaderChangeCbArgs=status_cb_arg)

    # Start watching status, need cbs and cb args
    ufm_status.start()

    while not ufmMainEvent.is_set():
        time.sleep(MASTER_CHECK_INTERVAL)

    ufm_status.stop()

    log.info(" ===> UFM is Stopped! <===")


if __name__ == '__main__':
    main()
