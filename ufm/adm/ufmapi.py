# ufmapi.py

import requests

REDFISH = "/redfish/v1"
g_ufm_address = ""

def cfg_ufm_address(address=None, port=None):
    global g_ufm_address

    if address != None:
        g_ufm_address = "http://"+address
    else:
        g_ufm_address = "http://127.0.0.0"

    if port != None:
        g_ufm_address += ":"+port

    return

#-------------------------------------------------------------------
# REDFISH Query Calls
#-------------------------------------------------------------------

def redfish_get(request, parms={}):
    global g_ufm_address

    try:
        response = requests.get(g_ufm_address+REDFISH+request, json=parms)

        if response.status_code != 200:
            print("ERROR: Redfish response code ", response.status_code)
            return None

        return response.json()

    except:
        print("Unable to contact UFM. (GET)")

    return None

def redfish_post(request, parms={}):
    global g_ufm_address

    try:
        response = requests.post(g_ufm_address+REDFISH+request, json=parms)

        if response.status_code != 200:
            print("ERROR: Redfish response code ", response.status_code)
            return None

        return response.json()
        
    except:
        print("Unable to contact UFM. (POST)")

    return None


def rf_get_systems():

    rsp = redfish_get("/Systems")

    if rsp == None:
        return None

    return rsp


def rf_get_system(system):

    rsp = redfish_get("/Systems/"+system)

    if rsp == None:
        return None

    return rsp


def rf_get_storage_list(subsystem):

    rsp = redfish_get("/Systems/"+subsystem+"/Storage")

    if rsp == None:
        return None

    return rsp


def rf_get_storage(subsystem, storage_id):

    rsp = redfish_get("/Systems/"+subsystem+"/Storage/"+storage_id)

    if rsp == None:
        return None

    return rsp


def rf_get_storage_drive(subsystem, storage_id, drive_id):

    rsp = redfish_get("/Systems/"+subsystem+"/Storage/"+storage_id+"/Drives/"+drive_id)

    if rsp == None:
        return None

    return rsp

def rf_get_managers():

    rsp = redfish_get("/Managers")

    if rsp == None:
        return None

    return rsp


def rf_get_manager(manager):

    rsp = redfish_get("/Managers/"+manager)

    if rsp == None:
        return None

    return rsp


def rf_get_ufm():

    rsp = redfish_get("/Managers/ufm")

    if rsp == None:
        return None

    return rsp


def rf_get_ufm_log():

    rsp = redfish_get("/Managers/ufm/LogServices/Log")

    if rsp == None:
        return None

    return rsp

#-------------------------------------------------------------------
# UFM Management
#-------------------------------------------------------------------


class UfmLogEntry(object):
    def __init__(self, redfish_log_entry):
        self.id = int(redfish_log_entry["Id"])
        self.msg = redfish_log_entry["Message"]
        self.module = redfish_log_entry["SensorType"]
        self.timestamp = float(redfish_log_entry["Created"])
        self.type = redfish_log_entry["EntryCode"]
        return

def ufm_get_log_entries(id, count):
    '''
    id = -1 indicates last <count> entries.
    Actual entries returned will be <= count.
    '''
    payload = {}
    rf_entries = []
    log_entries = []
    
    if id >= 1:
        payload["$skip"] = id

    payload["$top"]  = count

    rsp = redfish_get("/Managers/ufm/LogServices/Log/Actions/LogService.Entries", payload)

    if rsp == None:
        return None

    rf_entries = rsp['Members']

    if len(rf_entries) == 0:
        return []

    for rf_entry in rf_entries:
        log_entry = UfmLogEntry(rf_entry)
        log_entries.append(log_entry)

    return log_entries

def ufm_clear_log():
    rsp = redfish_post("/Managers/ufm/LogServices/Log/Actions/LogService.ClearLog", {})

    if rsp == None:
        return 1

    if rsp['Status'] != 200:
        print()
        print("ERROR: code("+str(rsp['Status'])+")", rsp['Message'])
        return 1

    return 0

def ufm_get_module_registry():
        rsp = redfish_get("/Managers/ufm/LogServices/Log/Actions/LogService.GetRegistry",{})

        if rsp == None:
            return None

        if rsp['Status'] != 200:
            print()
            print("ERROR: code("+str(rsp['Status'])+")", rsp['Message'])
            return None

        registry = rsp['Registry']
        return registry


def ufm_get_log_mask():
        payload = {"MaskType":"All"}

        rsp = redfish_get("/Managers/ufm/LogServices/Log/Actions/LogService.GetMask",\
            payload)

        if rsp == None:
            return None

        if rsp['Status'] != 200:
            print()
            print("ERROR: code("+str(rsp['Status'])+")", rsp['Message'])
            return None

        masks = {}
        masks['ErrorMask'] = rsp['ErrorMask']
        masks['WarningMask'] = rsp['WarningMask']
        masks['InfoMask'] = rsp['InfoMask']
        masks['DebugMask'] = rsp['DebugMask']
        masks['DetailMask'] = rsp['DetailMask']

        return masks

def ufm_set_log_mask(level, mask):
    '''
    level: <all, error, warning, info, debug, detail>
    mask: 0 to 0xFFFFFFFF
    '''
    payload = {}
    response = {}

    if level == "all":
        payload["ErrorMask"] = mask
        payload["WarningMask"] = mask
        payload["InfoMask"] = mask
        payload["DebugMask"] = mask
        payload["DetailMask"] = mask

    elif level == "error":
        payload["ErrorMask"] = mask

    elif level == "warning":
        payload["WarningMask"] = mask

    elif level == "info":
        payload["InfoMask"] = mask

    elif level == "debug":
        payload["DebugMask"] = mask

    elif level == "detail":
        payload["DetailMask"] = mask
    else:
        print("set_log_masks(): Invalid argument. (level= %s)" % level)
        return None

    rsp = redfish_post("/Managers/ufm/LogServices/Log/Actions/LogService.SetMask",\
        payload)

    if rsp == None:
        return None

    if rsp['Status'] != 200:
        print()
        print("ERROR: code("+str(rsp['Status'])+")", rsp['Message'])
        return None

    if "ErrorMask" in rsp:
        response['ErrorMask'] = rsp['ErrorMask']

    if "WarningMask" in rsp:
        response['WarningMask'] = rsp['WarningMask']

    if "InfoMask" in rsp:
        response['InfoMask'] = rsp['InfoMask']

    if "DebugMask" in rsp:
        response['DebugMask'] = rsp['DebugMask']

    if "DetailMask" in rsp:
        response['DetailMask'] = rsp['DetailMask']

    return response


def ufm_restart():
    payload = {"ResetType":"ForceRestart"}
    rsp = redfish_post("/Managers/ufm/Actions/Ufm.Reset",\
        payload)

    if rsp == None:
        return 1

    if rsp['Status'] != 200:
        print()
        print("ERROR: code("+str(rsp['Status'])+")", rsp['Message'])
        return 1

    return 0

def ufm_shutdown():
    payload = {"ResetType":"ForceOff"}
    rsp = redfish_post("/Managers/ufm/Actions/Ufm.Reset",\
        payload)

    if rsp == None:
        return 1

    if rsp['Status'] != 200:
        print()
        print("ERROR: code("+str(rsp['Status'])+")", rsp['Message'])
        return 1

    return 0
