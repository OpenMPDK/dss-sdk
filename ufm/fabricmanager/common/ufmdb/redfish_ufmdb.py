# redfish_ufmdb.py

import json
import pprint
import copy
import time
import subprocess
import uuid

from common.ufmdb.ufmdb import client
from common.ufmlog import ufmlog
from common.utils.ufm_decorators import singleton

g_ufmlog = None

redfish_responses = {
    '1':
    {
        "@odata.context": "/redfish/v1/$metadata#ServiceRoot.ServiceRoot",
        "@odata.type": "#ServiceRoot.v1_0_0.ServiceRoot",
        "@odata.id": "/redfish/v1",
        "Id": "RootService",
        "Name": "Root Service",
        "ProtocolFeaturesSupported": {
            "ExpandQuery": {
                "ExpandAll": False
            },
            "SelectQuery": False
        },
        "RedfishVersion": "1.8.0",
        "UUID": "None",
        "Vendor": "Samsung",
        "JSONSchemas": {
            "@odata.id": "/redfish/v1/JSONSchemas"
        },
        "Systems": {
            "@odata.id":"/redfish/v1/Systems"
        },
        "Managers": {
            "@odata.id":"/redfish/v1/Managers"
        },
    },

    '1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#ComputerSystemCollection.ComputerSystemCollection",
        "@odata.type": "#ComputerSystemCollection.ComputerSystemCollection",
        "@odata.id": "",
        "Description": "Collection of Computer Systems",
        "Members": list(),
        "Members@odata.count": 0,
        "Name": "System Collection"
    },

    '1.2':
    {
        "@odata.id": "/redfish/v1/JSONSchemas",
        "id": "http://redfish.dmtf.org/schemas/v1/redfish-schema.v1_2_0",
        "type": "object",
        "$schema": "http://redfish.dmtf.org/schemas/v1/redfish-schema.v1_2_0",
        "title": "Redfish Schema Extension",
        "description": "The properties defined in this schema shall adhere to the requirements of the Redfish Specification and the semantics of the descriptions in this file.",
        "allOf": [
            {
                "$ref": "http://json-schema.org/draft-04/schema"
            }
        ],
        "definitions": {
            "readonly": {
                "type": "boolean",
                "description": "This property shall designate a property to be readonly when set to true."
            },
            "requiredOnCreate": {
                "type": "array",
                "items": {
                    "type": "boolean"
                },
                "description": "This property is required to be specified in the body of a POST request to create the resource."
            },
            "longDescription": {
                "type": "string",
                "description": "This attribute shall contain normative language relating to the Redfish Specification and documentation."
            },
            "copyright": {
                "type": "string",
                "description": "This attribute shall contain the copyright notice for the schema."
            },
            "deprecated": {
                "type": "string",
                "description": "The term shall be applied to a property in order to specify that the property is deprecated.  The value of the string should explain the deprecation, including new property or properties to be used. The property can be supported in new and existing implementations, but usage in new implementations is discouraged.  Deprecated properties are likely to be removed in a future major version of the schema."
            },
            "enumDescriptions": {
                "type": "object",
                "description": "This attribute shall contain informative language related to the enumeration values of the property."
            },
            "enumLongDescriptions": {
                "type": "object",
                "description": "This attribute shall contain normative language relating to the enumeration values of the property."
            },
            "enumDeprecated": {
                "type": "object",
                "description": "The term shall be applied to a value in order to specify that the value is deprecated.  The value of the string should explain the deprecation, including new value to be used.  The value can be supported in new and existing implementations, but usage in new implementations is discouraged.  Deprecated values are likely to be removed in a future major version of the schema."
            },
            "units": {
                "type": "string",
                "description": "This attribute shall contain the units of measure used by the value of the property."
            }
        },
        "properties": {
            "readonly": {
                "$ref": "#/definitions/readonly"
            },
            "longDescription": {
                "$ref": "#/definitions/longDescription"
            },
            "copyright": {
                "$ref": "#/definitions/copyright"
            },
            "enumDescriptions": {
                "$ref": "#/definitions/enumDescriptions"
            },
            "enumLongDescriptions": {
                "$ref": "#/definitions/enumLongDescriptions"
            },
            "units": {
                "$ref": "#/definitions/units"
            }
        }
    },

    '1.3':
    {
        "@odata.context": "/redfish/v1/$metadata#ManagerCollection.ManagerCollection",
        "@odata.type": "#ManagerCollection.ManagerCollection",
        "@odata.id": "/redfish/v1/Managers",
        "Name": "Manager Collection",
        "Description": "Collection of Managers",
        "Members": [
            {
                "@odata.id":"/redfish/v1/Managers/ufm"
            }
        ],
        "Members@odata.count": 1,
    },

    '1.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#ComputerSystem.ComputerSystem",
        "@odata.type": "#ComputerSystem.v1_9_0.ComputerSystem",
        "@odata.id": "",
        "Description": "System containing subsystem(s)",
        "Id": "",
        "Identifiers": [
            {
                "DurableName": "",
                "DurableNameFormat": "UUID"
            }
        ],
        "IPv4Addresses": [
            {
                "Address": "",
                "SubnetMask": "",
                "AddressOrigin": "",
                "Gateway": ""
            }
        ],
        "IPv6DefaultGateway": "",
        "IPv6Addresses": [
            {
                "Address": "",
                "PrefixLength": 0,
                "AddressOrigin": "",
                "AddressState": ""
            }
        ],
        "Links": {
            "SupplyingComputerSystems": list()
        }
    },

    '1.1.2':
    {
        "@odata.context": "/redfish/v1/$metadata#ComputerSystem.ComputerSystem",
        "@odata.id": "",
        "@odata.type": "#ComputerSystem.v1_9_0.ComputerSystem",
        "Description": "System representing the drive resources",
        "Id": "",
        "Identifiers": [
            {
                "DurableName": "",
                "DurableNameFormat": "NQN"
            }
        ],
        "Status": {
            "State": "Enabled",
            "Health": "OK"
        },
        "Storage": dict(),
        "EthernetInterfaces": dict(),

        "Name": "System",
        "oem": dict(),
        "Links": {"ConsumingComputerSystems": list()}
    },

    '1.1.2.1':
    {
        "@odata.context": "/redfish/v1/$metadata#StorageCollection.StorageCollection",
        "@odata.type": "#StorageCollection.StorageCollection",
        "@odata.id": "",
        "Description": "Collection of Storage information",
        "Members": list(),
        "Members@odata.count": 0,
        "Name": "Storage Collection",
        "oem": dict(),
    },

    '1.1.2.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#Storage.Storage",
        "@odata.type": "Storage.v1_8_0.Storage",
        "@odata.id": "",
        "Id": "",
        "Description": "Storage information",
        "Drives": list(),
        "Name": "Storage",
        "oem": dict()
    },

    '1.1.2.1.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#DriveCollection.DriveCollection",
        "@odata.id": "",
        "@odata.type": "#DriveCollection.DriveCollection",
        "Description": "Collection of drives",
        "Members": list(),
        "Members@odata.count": 0,
        "Name": "Drive Collection"
    },

    '1.1.2.1.1.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#Drive.Drive",
        "@odata.type": "Drive.v1_8_0.Drive",
        "@odata.id": "",
        "Description": "Drive Information",
        "Name": "Storage Drive Information",
        "BlockSizeBytes": 512,
        "CapacityBytes": 0,
        "Id": "",
        "Manufacturer": "",
        "MediaType": "",
        "Model": "",
        "Protocol": "",
        "Revision":"",
        "SerialNumber": "",
        "oem": dict()
    },

    '1.1.2.2':
    {
        "@odata.context": "/redfish/v1/$metadata#EthernetInterfaceCollection.EthernetInterfaceCollection",
        "@odata.type": "#EthernetInterfaceCollection.EthernetInterfaceCollection",
        "@odata.id": "",
        "Description": "Collection of Ethernet Interfaces",
        "Members": list(),
        "Members@odata.count": 0,
        "Name": "Ethernet interfaces Collection"
    },

    '1.1.2.2.1':
    {
        "@odata.context": "/redfish/v1/$metadata#EthernetInterface.EthernetInterface",
        "@odata.type": "#EthernetInterface.v1_5_1.EthernetInterface",
        "@odata.id": "",
        "Name": "Ethernet Interface",
        "Description": "Ethernet Interface information",
        "Id": "",
        "LinkStatus": "",
        "MACAddress": "",
        "SpeedMbps": 0,
        "IPv4Addresses": [
            {
                "Address": "",
                "SubnetMask": "",
                "AddressOrigin": "",
                "Gateway": "",
                "oem": dict({'Port':0,'SupportedProtocol':""})
            }
        ],
        "IPv6DefaultGateway": "",
        "IPv6Addresses": [
            {
                "Address": "",
                "PrefixLength": 0,
                "AddressOrigin": "",
                "AddressState": "",
                "oem": dict({'Port':0,'SupportedProtocol':""})
            }
        ]
    },

    '1.3.1':
    {
        "@odata.context": "/redfish/v1/$metadata#UFM.v1_0_0.UFM",
        "@odata.type": "##UFM.v1_0_0.UFM",
        "@odata.id": "/redfish/v1/Managers/ufm",
        "Id": "UFM",
        "Name": "UFM System Manager",
        "UUID": "",
        "LogServices": {
            "@odata.id": "/redfish/v1/Managers/ufm/LogServices",
        },
        "Actions": {
            "#Ufm.Reset": {
                "target": "/redfish/v1/Managers/ufm/Actions/Ufm.Reset",
                "ResetType@Redfish.AllowableValues": [
                    "ForceOff",
                    "ForceRestart"]
                },
            }
    },
    '1.3.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#LogServiceCollection.LogServiceCollection",
        "@odata.type": "#LogServiceCollection.LogServiceCollection",
        "@odata.id": "/redfish/v1/Managers/ufm/LogServices",
        "Name":"Log Service Collection",
        "Description":"Collection of Log Services for UFM Manager",
        "Members": [
            {
                "@odata.id":"/redfish/v1/Managers/ufm/LogServices/Log"
            }
        ],
        "Members@odata.count": 1,
    },
    '1.3.1.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#LogServices.v1_1_0.LogServices",
        "@odata.type": "#LogServices.v1_1_0.LogServices",
        "@odata.id": "/redfish/v1/Managers/ufm/LogServices/Log",
        "Id":"Log",
        "Name":"UFM Log Services",
        "MaxNumberOfRecords": 1000,
        "Oem":{},
        "OverWritePolicy": "WrapsWhenFull",
        "Actions": {
            "#LogService.ClearLog": {
                "target": "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.ClearLog",
            },
            "#LogService.Entries": {
                "target": "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.Entries",
                #"@Redfish.ActionInfo": "/redfish/v1/Managers/ufm/LogServices/Log/EntriesActionInfo"
            },
            "#LogService.GetMask": {
                "target": "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.GetMask",
                "@Redfish.ActionInfo": "/redfish/v1/Managers/ufm/LogServices/Log/GetMaskActionInfo"
            },
            "#LogService.SetMask": {
                "target": "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.SetMask",
                "@Redfish.ActionInfo": "/redfish/v1/Managers/ufm/LogServices/Log/SetMaskActionInfo"
            },
            "#LogService.GetRegistry": {
                "target": "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.GetRegistry",
            },
        }
    },

    '1.3.1.1.1.1':
    {
        "@odata.context": "/redfish/v1/$metadata#LogEntryCollection.LogEntryCollection",
        "@odata.type": "#LogEntryCollection.LogEntryCollection",
        "@odata.id": "/redfish/v1/Managers/ufm/LogServices/Log/Entries",
        "Name": "Log Service Collection",
        "Description": "Collection of Logs for this System",
        "Members": [],
        "Members@odata.count": 0,
    },

    '1.3.1.1.1.2':
    {
        "@odata.context": "/redfish/v1/$metadata#ActionInfo.v1_1_0.ActionInfo",
        "@odata.type": "#ActionInfo.v1_1_0.ActionInfo",
        "@odata.id": "/redfish/v1/Managers/ufm/LogServices/Log/SetMaskActionInfo",
        "Id":"SetMaskActionInfo",
        "Name":"Set Mask Action Info",
        "Parameters": [
            {
            "Name": "ErrorMask",
            "Required": False,
            "DataType": "number",
            "MinimumValue" : 0,
            "MaximumValue" : 0xFFFFFFFF},
            {
            "Name": "WarningMask",
            "Required": False,
            "DataType": "number",
            "MinimumValue" : 0,
            "MaximumValue" : 0xFFFFFFFF},
            {
            "Name": "InfoMask",
            "Required": False,
            "DataType": "number",
            "MinimumValue" : 0,
            "MaximumValue" : 0xFFFFFFFF},
            {
            "Name": "DebugMask",
            "Required": False,
            "DataType": "number",
            "MinimumValue" : 0,
            "MaximumValue" : 0xFFFFFFFF},
            {
            "Name": "DetailMask",
            "Required": False,
            "DataType": "number",
            "MinimumValue" : 0,
            "MaximumValue" : 0xFFFFFFFF},
        ]
    },
    '1.3.1.1.1.3':
    {
        "@odata.context": "/redfish/v1/$metadata#ActionInfo.v1_1_0.ActionInfo",
        "@odata.type": "#ActionInfo.v1_1_0.ActionInfo",
        "@odata.id": "/redfish/v1/Managers/ufm/LogServices/Log/GetMaskActionInfo",
        "Id":"GetMaskActionInfo",
        "Name":"Get Mask Action Info",
        "Parameters": [
            {
            "Name": "MaskType",
            "Required": True,
            "DataType": "string",
            "AllowableValues": [
                "All",
                "ErrorMask",
                "WarningMask",
                "InfoMask",
                "DebugMask",
                "DetailMask"
                ],
            },
        ]
    },
}


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


def ufm_clearlog_action(payload):
    global g_ufmlog

    g_ufmlog.ufmlog.clear_log()

    response = { "Status": 200, \
        "Message": "Successfully completed request." }

    return response

def ufm_entries_action(payload):
    global g_ufmlog
    offset = None
    count = None

    if "$skip" in payload:
        offset = int(payload["$skip"]) + 1

    if "$top" in payload:
        count = int(payload["$top"])

    log_entries = g_ufmlog.ufmlog.get_entries(offset, count)

    response = copy.deepcopy(redfish_responses['1.3.1.1.1.1'])
    response['Members'] = redfish_log_entries(log_entries)
    response['Members@odata.count'] = len(response['Members'])

    return response

def redfish_log_entries(entries):
    '''
    Converts a list of UfmLogEntry objects to a list of Redfish LogEntry objects
    '''
    log_entries = []
    for entry in entries:
        LogEntry = {}
        LogEntry["@odata.context"] =  "/redfish/v1/$metadata#LogEntry.LogEntry"
        LogEntry["@odata.id"] =  "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.Entries"
        LogEntry["@odata.type"] =  "#LogEntry.v1_3_0.LogEntry"
        LogEntry["Id"] =  str(entry.id)
        LogEntry["Name"] =  "Log Entry "+str(entry.id)
        LogEntry["EntryType"] =  "Oem"
        LogEntry["OemRecordFormat"] =  "Samsung"

        if entry.type == "ERROR":
            LogEntry["Severity"] =  "Warning"
        if entry.type == "WARNING":
            LogEntry["Severity"] =  "Warning"
        if entry.type == "INFO":
            LogEntry["Severity"] =  "OK"
        if entry.type == "DEBUG":
            LogEntry["Severity"] =  "OK"
        if entry.type == "DETAIL":
            LogEntry["Severity"] =  "OK"
        if entry.type == "EXCEPT":
            LogEntry["Severity"] =  "Critical"

        LogEntry["Created"] =  str(entry.timestamp)
        LogEntry["SensorType"] =  entry.module
        LogEntry["EntryCode"] =  entry.type
        LogEntry["Message"] =  entry.msg

        log_entries.append(LogEntry)

    return log_entries

'''
Redfish LogEntry example
{
"@odata.type": "#LogEntry.v1_3_0.LogEntry",
"Id": "1",
"Name": "Log Entry 1",
"EntryType": "Oem",
"OemRecordFormat": "Samsung",
"Severity": "Critical",
"Created": "2012-03-07T14:44:00Z",
"EntryCode": "Assert",
"SensorType": "Temperature",
"SensorNumber": 1,
"Message": "Temperature threshold exceeded",
"MessageId": "Contoso.1.0.TempAssert",
"MessageArgs": [
"42"
],
"Links": {
"OriginOfCondition": {
"@odata.id": "/redfish/v1/Chassis/1U/Thermal"
},
"Oem": {}
},
"Oem": {},
"@odata.context": "/redfish/v1/$metadata#LogEntry.LogEntry",
"@odata.id": "/redfish/v1/Systems/437XR1138R2/LogServices/Log1/Entries/1"
}
'''

def ufm_getregistry_action(payload):
    global g_ufmlog
    registry = g_ufmlog.ufmlog.get_module_registry()

    response = { "Status": 200, \
        "Message": "Successfully completed request.",
        "Registry": registry }

    return response


def ufm_getmask_action(payload):
    global g_ufmlog

    if payload["MaskType"] == "All":
        error_mask = int(g_ufmlog.ufmlog.get_log_error())
        warning_mask = int(g_ufmlog.ufmlog.get_log_warning())
        info_mask = int(g_ufmlog.ufmlog.get_log_info())
        debug_mask = int(g_ufmlog.ufmlog.get_log_debug())
        detail_mask = int(g_ufmlog.ufmlog.get_log_detail())

        response = { "Status": 200, \
            "Message": "Successfully completed request.", \
            "ErrorMask": error_mask,
            "WarningMask": warning_mask,
            "InfoMask": info_mask,
            "DebugMask": debug_mask,
            "DetailMask": detail_mask }

    elif payload["MaskType"] == "ErrorMask":
        error_mask = g_ufmlog.ufmlog.get_log_error()
        response = { "Status": 200, \
            "Message": "Successfully completed request.", \
            "ErrorMask": error_mask}

    elif payload["MaskType"] == "WarningMask":
        warning_mask = g_ufmlog.ufmlog.get_log_warning()
        response = { "Status": 200, \
            "Message": "Successfully completed request.", \
            "WarningMask": warning_mask}

    elif payload["MaskType"] == "InfoMask":
        info_mask = g_ufmlog.ufmlog.get_log_info()
        response = { "Status": 200, \
            "Message": "Successfully completed request.", \
            "InfoMask": info_mask}

    elif payload["MaskType"] == "DebugMask":
        debug_mask = g_ufmlog.ufmlog.get_log_debug()
        response = { "Status": 200, \
            "Message": "Successfully completed request.", \
            "DebugMask": debug_mask}

    elif payload["MaskType"] == "DetailMask":
        detail_mask = g_ufmlog.ufmlog.get_log_detail()
        response = { "Status": 200, \
            "Message": "Successfully completed request.", \
            "DetailMask": detail_mask}

    return response

def ufm_setmask_action(payload):
    global g_ufmlog

    response = { "Status": 200, \
            "Message": "Successfully completed request."}

    if "ErrorMask" in payload:
        value = payload["ErrorMask"]
        g_ufmlog.ufmlog.set_log_error(value)
        value = g_ufmlog.ufmlog.get_log_error()
        response["ErrorMask"] = value

    if "WarningMask" in payload:
        value = payload["WarningMask"]
        g_ufmlog.ufmlog.set_log_warning(value)
        value = g_ufmlog.ufmlog.get_log_warning()
        response["WarningMask"] = value

    if "InfoMask" in payload:
        value = payload["InfoMask"]
        g_ufmlog.ufmlog.set_log_info(value)
        value = g_ufmlog.ufmlog.get_log_info()
        response["InfoMask"] = value

    if "DebugMask" in payload:
        value = payload["DebugMask"]
        g_ufmlog.ufmlog.set_log_debug(value)
        value = g_ufmlog.ufmlog.get_log_debug()
        response["DebugMask"] = value

    if "DetailMask" in payload:
        value = payload["DetailMask"]
        g_ufmlog.ufmlog.set_log_detail(value)
        value = g_ufmlog.ufmlog.get_log_detail()
        response["DetailMask"] = value

    return response

def ufm_reset_action(payload):
    global g_ufmlog

    type = payload['ResetType']

    if type == 'ForceOff':
        g_ufmlog.info("SHUTDOWN: requested.")

        cmd = "python ufm.py stop &"
        subprocess.call(cmd, shell=True)

        response = { "Status": 200, "Message": "Successfully requested shutdown." }

    elif type == 'ForceRestart':
        g_ufmlog.info("RESTART: requested.")

        cmd = "python ufm.py restart &"
        subprocess.call(cmd, shell=True)

        response = { "Status": 200, "Message": "Successfully requested restart." }

    else:
        response = { "Status": 400, "Message": "Bad Request" }

    return response


@singleton
class RedfishUfmdb(object):
    root_uuid = str(uuid.uuid4())


    def __init__(self, auto_update=True, expire=5):
        """
        Create connection to database.
        """
        global g_ufmlog

        self.log = ufmlog.log(module="RFDB", mask=ufmlog.UFM_REDFISH_DB)
        g_ufmlog = self.log
        self.log.log_detail_on()
        #self.pp = pprint.PrettyPrinter(indent=4, sort_dicts=False)

        self.auto_update = auto_update

        self.ufmdb = client(db_type = 'etcd')
        self.redfish = dict()
        self.systems = list()
        self.action = dict()

        self.data_expiration = 0.0
        self.expiration = float(expire)

        if self.auto_update == False:
            self.update()

    def pprint(self):
        print("SYSTEMS:")
        for s in self.systems:
            s.pprint()

    def _query_prefix(self, qstring):
        kv_dict = dict()

        query = self.ufmdb.get_prefix(qstring)

        if query == None:
            self.log.error('get_prefix(\"%s\") failed.  Retrying...', qstring)
            query = self.ufmdb.get_prefix(qstring)
            if query == None:
                self.log.error('get_prefix(\"%s\") retry failed. Exiting.', qstring)
                return dict()

        for value, metadata in query:
            kv_dict.update({metadata.key.decode('utf-8'):value.decode('utf-8')})

        return(kv_dict)

    def update(self):
        # Update action URL database
        self.log.detail('Update actions: requested.  Updating now ...')
        del(self.action)
        self.action = {}
        self._build_redfish_actions()

        # Update get URL database
        current_time = time.time()
        attempts = 2
        while True:
            try:
                if (current_time > self.data_expiration):
                    self.log.detail('Update data: requested.  Updating now ...')
                    self.data_expiration = current_time + self.expiration

                    del(self.redfish)
                    del(self.systems)

                    self.redfish = {}
                    self.systems = []

                    self._process_database()
                    self._build_redfish_root()
                    self._build_redfish_systems()

                else:
                    self.log.detail('Update data: requested.  Data expires in %d second(s).', 1 + int(self.data_expiration - current_time))
                break

            except Exception as e:
                attempts -= 1
                if attempts == 0:
                    break

                self.log.exception(e)
                self.data_expiration = 0.0
                self.log.error("Unable to update data.  Retrying...")

        return

    def _process_database(self):

        self.log.detail('_process_database: requested.')

        systems = self._query_prefix("/object_storage/servers/list")

        if len(systems) == 0:
            self.log.detail('_process_database: done.')
            return

        for key in systems:
            sys_list = key.split("/")
            sys_uuid = sys_list[4]
            sys = system(uuid=sys_uuid)
            self.systems.append(sys)

            subsystems = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/")

            target_key = "/object_storage/servers/"+sys_uuid+"/server_attributes/identity/Hostname"
            target_dict = self._query_prefix(target_key)

            for ss_key in subsystems:
                if ss_key.find("UUID") != -1:
                    ss_uuid = sys_uuid+"."+subsystems[ss_key]
                    subsys = subsystem(uuid=ss_uuid)
                    sys.subsystems.append(subsys)

                    ss_list = ss_key.split("/")
                    subsys.nqn  = ss_list[7]
                    subsys.name = subsystems[ss_key]
                    subsys.servername = target_dict.get(target_key,"")

                    # Set the NQN ID for the subsystem
                    subsys.nsid = 1 #Set the subsystem nsid to the default of 1 for now. Needs to be revisited

                    transports = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/"+subsys.nqn+"/transport_addresses/")
                    for transport_key in transports:
                        if transport_key.find("traddr") != -1:
                            intr = interface()
                            subsys.interfaces.append(intr)
                            line_list = transport_key.split("/")
                            intr.mac = line_list[9]
                            intr.name = line_list[9]
                            traddr = transports[transport_key]

                            # Read status of ip interface
                            tran_mac = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/network/interfaces/"+intr.mac+"/Status")
                            for tran_mac_key in tran_mac:
                                if tran_mac_key.find("Status") != -1:
                                    status = tran_mac[tran_mac_key]
                                    if status == 'up':
                                        intr.status = "LinkUp"
                                    elif status == 'down':
                                        intr.status = "LinkDown"
                                    else:
                                        intr.status = "NoLink"

                            sub_attrs = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/"+subsys.nqn)
                            for sub_attr_key in sub_attrs:
                                if sub_attr_key.find("numa_aligned") != -1:
                                    numa = sub_attrs[sub_attr_key]
                                    if (int(numa) == 1):
                                        subsys.numa_aligned = True
                                    else:
                                        subsys.numa_aligned = False

                            # Read remaining info about interface
                            tran_mac = self._query_prefix("/object_storage/servers/"+sys_uuid+"/kv_attributes/config/subsystems/"+subsys.nqn+"/transport_addresses/"+intr.mac)
                            for tran_mac_key in tran_mac:
                                if tran_mac_key.find("adrfam") != -1:
                                    adrfam = tran_mac[tran_mac_key]
                                    if adrfam == "IPv4":
                                        ip4 = ipv4()
                                        ip4.addr = traddr
                                        intr.ip4_intf.append(ip4)
                                    elif adrfam == "IPv6":
                                        ip6 = ipv6()
                                        ip6.addr = traddr
                                        intr.ip6_intf.append(ip6)

                                if tran_mac_key.find("trsvcid") != -1:
                                    trsvcid = tran_mac[tran_mac_key]
                                    intr.port = int(trsvcid)

                                if tran_mac_key.find("trtype") != -1:
                                    trtype = tran_mac[tran_mac_key]
                                    intr.type = trtype

                                if tran_mac_key.find("interface_speed") != -1:
                                    speed = tran_mac[tran_mac_key]
                                    intr.speed = int(speed)

            # create a storage object
            stor = storage()

            # set its uuid to the hostname
            storage_attr = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/identity/Hostname")
            for attr_key in storage_attr:
                if attr_key.find("Hostname") != -1:
                    stor.uuid = storage_attr[attr_key]
                    stor.uuid = stor.uuid.split('.')[0]

            # Find and add devices to storage object
            storage_devices = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/storage/nvme/devices/")
            for device_key in storage_devices:
                if device_key.find("Serial") != -1:
                    device_sn = storage_devices[device_key]
                    drv = drive()
                    stor.drives.append(drv)

                    dev_info = self._query_prefix("/object_storage/servers/"+sys_uuid+"/server_attributes/storage/nvme/devices/"+device_sn)
                    for device_info_key in dev_info:
                        drv.uuid = device_sn
                        drv.sn = device_sn
                        drv.protocol = 'NVMeOverFabrics'
                        drv.manufacturer = 'Samsung'            #????
                        drv.type = 'SSD'                        #????

                        if device_info_key.find("LogicalBlockSize") != -1:
                            drv.block_size = int(dev_info[device_info_key])
                            continue
                        if device_info_key.find("Model") != -1:
                            model_list = dev_info[device_info_key].split(' ')

                            if len(model_list) > 1:
                                drv.model = model_list[1]
                            else:
                                drv.model = model_list[0]
                            continue
                        if device_info_key.find("FirmwareRevision") != -1:
                            drv.revision = dev_info[device_info_key]
                            continue
                        if device_info_key.find("DiskCapacityInBytes") != -1:
                            drv.capacity = int(dev_info[device_info_key])
                            continue
                        if device_info_key.find("DiskUtilizationInBytes") != -1:
                            drv.utilization = int(dev_info[device_info_key])
                            continue

                    # Sanity check the drive data. There is a known issue with this
                    if drv.utilization > drv.capacity:
                        drv.utilization = 0

                    drv.percent_avail = int((1.0 - (drv.utilization/drv.capacity)) * 100)

                    stor.capacity = stor.capacity + drv.capacity
                    stor.utilization = stor.utilization + drv.utilization

            stor.percent_avail = int((1.0 - (stor.utilization/stor.capacity)) * 100)

            # Find the subsystem to attach this storage
            for subsys in sys.subsystems:
                nqn = subsys.nqn.lower()
                uuid = stor.uuid.lower()
                if nqn.find(uuid) != -1:
                    subsys.storage.append(stor)
                    subsys.capacity = subsys.capacity + stor.capacity
                    subsys.utilization = subsys.utilization + stor.utilization


        # Calculate subsystem storage level data
        for subsys in sys.subsystems:
                if len(subsys.storage):
                    subsys.percent_avail = int((1.0 - (subsys.utilization/subsys.capacity)) * 100)

        self.log.detail('_process_database: done.')

        return

    def _build_redfish_actions(self):
        self.log.detail('_build_redfish_actions: requested.')

        self.action['/redfish/v1/Managers/ufm/Actions/Ufm.Reset'] = ufm_reset_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.ClearLog'] = ufm_clearlog_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.Entries'] = ufm_entries_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.GetMask'] = ufm_getmask_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.SetMask'] = ufm_setmask_action
        self.action['/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.GetRegistry'] = ufm_getregistry_action

        self.log.detail('_build_redfish_actions: done.  entries=%d',len(self.action))
        return

    def _build_redfish_root(self):

        self.log.detail('_build_redfish_root: requested.')

        # 1
        response_1 = copy.deepcopy(redfish_responses['1'])
        response_1['UUID'] = self.root_uuid
        self.redfish[response_1['@odata.id']] = response_1   # 1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        if  len(self.systems) == 0:
            self.log.detail('_build_redfish_root: done.  entries=%d',len(self.redfish))
            return

        self.log.detail('_build_redfish_root: done.  entries=%d',len(self.redfish))
        return

    def _build_redfish_systems(self):

        self.log.detail('_build_redfish_systems: requested.')

        if  len(self.systems) == 0:
            self.log.detail('_build_redfish_systems: done.  entries=%d',len(self.redfish))
            return

        # 1.1
        response_1_1 = copy.deepcopy(redfish_responses['1.1'])
        response_1_1['@odata.id'] = '/redfish/v1/Systems'

        for sys in self.systems:
            response_1_1['Members'].append({'@odata.id': '/redfish/v1/Systems/'+sys.uuid})

            # Add the subsystems uuids too
            for subsys in sys.subsystems:
                response_1_1['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid})

        response_1_1['Members@odata.count'] = len(response_1_1['Members'])
        self.redfish[response_1_1['@odata.id']] = response_1_1  # 1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.2
        response_1_2 = copy.deepcopy(redfish_responses['1.2'])
        self.redfish[response_1_2['@odata.id']] = response_1_2  # 1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3
        response_1_3 = copy.deepcopy(redfish_responses['1.3'])
        self.redfish[response_1_3['@odata.id']] = response_1_3  # 1.3   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1
        response_1_3_1 = copy.deepcopy(redfish_responses['1.3.1'])
        response_1_3_1['UUID'] = sys.uuid
        self.redfish[response_1_3_1['@odata.id']] = response_1_3_1  # 1.3.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1.1
        response_1_3_1_1 = copy.deepcopy(redfish_responses['1.3.1.1'])
        self.redfish[response_1_3_1_1['@odata.id']] = response_1_3_1_1  # 1.3.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1.1.1
        response_1_3_1_1_1 = copy.deepcopy(redfish_responses['1.3.1.1.1'])
        response_1_3_1_1_1['MaxNumberOfRecords'] = g_ufmlog.ufmlog.max_entries
        self.redfish[response_1_3_1_1_1['@odata.id']] = response_1_3_1_1_1  # 1.3.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        '''
        # 1.3.1.1.1.1
        response_1_3_1_1_1_1 = copy.deepcopy(redfish_responses['1.3.1.1.1.1'])
        self.redfish[response_1_3_1_1_1_1['@odata.id']] = response_1_3_1_1_1_1  # 1.3.1.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
        '''

        # 1.3.1.1.1.2
        response_1_3_1_1_1_2 = copy.deepcopy(redfish_responses['1.3.1.1.1.2'])
        self.redfish[response_1_3_1_1_1_2['@odata.id']] = response_1_3_1_1_1_2  # 1.3.1.1.1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.3.1.1.1.3
        response_1_3_1_1_1_3 = copy.deepcopy(redfish_responses['1.3.1.1.1.3'])
        self.redfish[response_1_3_1_1_1_3['@odata.id']] = response_1_3_1_1_1_3  # 1.3.1.1.1.3   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        # 1.1.1
        for sys in self.systems:
            response_1_1_1 = copy.deepcopy(redfish_responses['1.1.1'])

            response_1_1_1['@odata.id'] = '/redfish/v1/Systems/'+sys.uuid
            response_1_1_1['Id'] = sys.uuid

            ident  = response_1_1_1['Identifiers'][0]
            ident['DurableName'] = sys.uuid

            ipv4  = response_1_1_1['IPv4Addresses'][0]
            ipv6  = response_1_1_1['IPv6Addresses'][0]
            links  = response_1_1_1['Links']

            # Add ip addresses and links
            for subsys in sys.subsystems:
                for interface in subsys.interfaces:
                    for ip4 in interface.ip4_intf:
                        ipv4['Address'] = ip4.addr
                        break
                    for ip6 in interface.ip6_intf:
                        ipv6['Address'] = ip6.addr
                        break
                links['SupplyingComputerSystems'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid})

            self.redfish[response_1_1_1['@odata.id']] = response_1_1_1  # 1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

            for subsys in sys.subsystems:
                response_1_1_2 = copy.deepcopy(redfish_responses['1.1.2'])

                response_1_1_2['@odata.id'] = '/redfish/v1/Systems/'+subsys.uuid
                response_1_1_2['Id'] = subsys.uuid

                ident  = response_1_1_2['Identifiers'][0]
                ident['DurableName'] = subsys.nqn

                if len(subsys.storage) != 0:
                    response_1_1_2['Storage'] = ({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage"})

                if len(subsys.interfaces) != 0:
                    response_1_1_2['EthernetInterfaces'] = ({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces"})

                response_1_1_2['oem'] = { "ServerName":subsys.servername, "NSID":subsys.nsid, "NumaAligned":subsys.numa_aligned }

                response_1_1_2['Links']['ConsumingComputerSystems'].append({"@odata.id":"/redfish/v1/Systems/"+sys.uuid})

                self.redfish[response_1_1_2['@odata.id']] = response_1_1_2  # 1.1.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                if len(subsys.storage) != 0:
                    response_1_1_2_1 = copy.deepcopy(redfish_responses['1.1.2.1'])
                    response_1_1_2_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage"

                    for stor in subsys.storage:
                        response_1_1_2_1['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid})

                    response_1_1_2_1['Members@odata.count'] = len(response_1_1_2_1['Members'])
                    response_1_1_2_1['oem'] = { "CapacityBytes":subsys.capacity, "UtilizationBytes":subsys.utilization,"PercentAvailable":subsys.percent_avail}

                    self.redfish[response_1_1_2_1['@odata.id']] = response_1_1_2_1  # 1.1.2.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


                    response_1_1_2_1_1 = copy.deepcopy(redfish_responses['1.1.2.1.1'])
                    response_1_1_2_1_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid
                    response_1_1_2_1_1['Id'] = stor.uuid

                    for drive in stor.drives:
                        response_1_1_2_1_1['Drives'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives/"+drive.uuid})

                    response_1_1_2_1_1['oem'] = { "CapacityBytes":stor.capacity, "UtilizationBytes":stor.utilization, "PercentAvailable":stor.percent_avail}

                    self.redfish[response_1_1_2_1_1['@odata.id']] = response_1_1_2_1_1  # 1.1.2.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


                    response_1_1_2_1_1_1 = copy.deepcopy(redfish_responses['1.1.2.1.1.1'])
                    response_1_1_2_1_1_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives"
                    response_1_1_2_1_1_1['Id'] = stor.uuid

                    for drive in stor.drives:
                        response_1_1_2_1_1_1['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives/"+drive.uuid})

                    response_1_1_2_1_1_1['Members@odata.count'] = len(response_1_1_2_1_1_1['Members'])
                    self.redfish[response_1_1_2_1_1_1['@odata.id']] = response_1_1_2_1_1_1  # 1.1.2.1.1.1   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                    for drive in stor.drives:
                        response_1_1_2_1_1_1_1 = copy.deepcopy(redfish_responses['1.1.2.1.1.1.1'])
                        response_1_1_2_1_1_1_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/Storage/"+stor.uuid+"/Drives/"+drive.uuid
                        response_1_1_2_1_1_1_1['Id'] = drive.uuid
                        response_1_1_2_1_1_1_1['BlockSizeBytes'] = drive.block_size
                        response_1_1_2_1_1_1_1['CapacityBytes'] = drive.capacity
                        response_1_1_2_1_1_1_1['Id'] = drive.uuid
                        response_1_1_2_1_1_1_1['Manufacturer'] = drive.manufacturer
                        response_1_1_2_1_1_1_1['MediaType'] = drive.type
                        response_1_1_2_1_1_1_1['Model'] = drive.model
                        response_1_1_2_1_1_1_1['Protocol'] = drive.protocol
                        response_1_1_2_1_1_1_1['Revision'] = drive.revision
                        response_1_1_2_1_1_1_1['SerialNumber'] = drive.sn
                        response_1_1_2_1_1_1_1['oem'] = {"UtilizationBytes":drive.utilization, "PercentAvailable":drive.percent_avail}

                        self.redfish[response_1_1_2_1_1_1_1['@odata.id']] = response_1_1_2_1_1_1_1  # 1.1.2.1.1.1.1  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


                if len(subsys.interfaces) != 0:
                    response_1_1_2_2 = copy.deepcopy(redfish_responses['1.1.2.2'])
                    response_1_1_2_2['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces"

                    for intf in subsys.interfaces:
                        response_1_1_2_2['Members'].append({"@odata.id": "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces/"+intf.mac})
                    response_1_1_2_2['Members@odata.count'] = len(response_1_1_2_2['Members'])
                    #print("members=", response_1_1_2_2['Members@odata.count'])
                    self.redfish["/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces"] = response_1_1_2_2  # 1.1.2.2   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                    for intf in subsys.interfaces:
                        response_1_1_2_2_1 = copy.deepcopy(redfish_responses['1.1.2.2.1'])
                        response_1_1_2_2_1['@odata.id'] = "/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces/"+intf.mac
                        response_1_1_2_2_1['Id'] = intf.mac
                        response_1_1_2_2_1['SpeedMbps'] = intf.speed
                        response_1_1_2_2_1['MACAddress'] = intf.mac
                        response_1_1_2_2_1['LinkStatus'] = intf.status

                        ipv4  = response_1_1_2_2_1['IPv4Addresses'][0]
                        for ip4 in intf.ip4_intf:
                            ipv4['Address'] = ip4.addr
                            ipv4['oem']['Port'] = intf.port
                            ipv4['oem']['SupportedProtocol'] = intf.type
                            break

                        ipv6  = response_1_1_2_2_1['IPv6Addresses'][0]
                        for ip6 in intf.ip6_intf:
                            ipv6['Address'] = ip6.addr
                            ipv4['oem']['Port'] = intf.port
                            ipv4['oem']['SupportedProtocol'] = intf.type
                            break

                        self.redfish["/redfish/v1/Systems/"+subsys.uuid+"/EthernetInterfaces/"+intf.mac] = response_1_1_2_2_1  # 1.1.2.2.1  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        self.log.detail('_build_redfish_systems: done.  entries=%d',len(self.redfish))
        return

    def get(self, request=None, payload={}):
        '''
        Translates a redfish request string request to a json file response
        '''
        self.log.detail('GET: %s', request)

        try:
            if self.auto_update == True:
                self.update()

            if request in self.action:
                func = self.action[request]
                response = func(payload)

            elif request in self.redfish:
                response = self.redfish[request]

            else:
                if request != "/favicon.ico":
                    self.log.error('GET: Invalid Request. %s', request)
                response = { "Status": 404, "Message": "Not Found" }

            return response
        except Exception as e:
            self.log.exception(e)

            if request != "/favicon.ico":
                self.log.error('GET: Invalid Request. %s', request)

            response = { "Status": 404, "Message": "Not Found" }
            return response

    def post(self, request=None, payload={}):
        '''
        Translates a redfish request string request to an action function
        '''
        self.log.detail('POST: %s', request)

        try:
            if self.auto_update == True:
                self.update()

            if request in self.action:
                func = self.action[request]
                response = func(payload)

            else:
                if request != "/favicon.ico":
                    self.log.error('GET: Invalid Request. %s', request)
                response = { "Status": 404, "Message": "Not Found" }

            return response
        except:
            if request != "/favicon.ico":
                self.log.error('GET: Invalid Request. %s', request)

            response = { "Status": 404, "Message": "Not Found" }
            return response





