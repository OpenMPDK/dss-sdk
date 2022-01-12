# The Clear BSD License
#
# Copyright (c) 2022 Samsung Electronics Co., Ltd.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the disclaimer
# below) provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Samsung Electronics Co., Ltd. nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
# THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
# NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# redfish_action.py

import copy
import subprocess

# from common.ufmlog import ufmlog
from common.ufmdb.redfish.redfish_responses import redfish_responses

g_ufmlog = None


def ufm_clearlog_action(payload):
    global g_ufmlog

    g_ufmlog.ufmlog.clear_log()

    response = {"Status": 200, "Message": "Successfully completed request."}

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
        LogEntry["@odata.context"] = "/redfish/v1/$metadata#LogEntry.LogEntry"
        LogEntry["@odata.id"] = "/redfish/v1/Managers/ufm/LogServices/Log/Actions/LogService.Entries"
        LogEntry["@odata.type"] = "#LogEntry.v1_3_0.LogEntry"
        LogEntry["Id"] = str(entry.id)
        LogEntry["Name"] = "Log Entry "+str(entry.id)
        LogEntry["EntryType"] = "Oem"
        LogEntry["OemRecordFormat"] = "Samsung"

        if entry.type == "ERROR":
            LogEntry["Severity"] = "Warning"
        if entry.type == "WARNING":
            LogEntry["Severity"] = "Warning"
        if entry.type == "INFO":
            LogEntry["Severity"] = "OK"
        if entry.type == "DEBUG":
            LogEntry["Severity"] = "OK"
        if entry.type == "DETAIL":
            LogEntry["Severity"] = "OK"
        if entry.type == "EXCEPT":
            LogEntry["Severity"] = "Critical"

        LogEntry["Created"] = str(entry.timestamp)
        LogEntry["SensorType"] = entry.module
        LogEntry["EntryCode"] = entry.type
        LogEntry["Message"] = entry.msg

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

    response = {"Status": 200,
                "Message": "Successfully completed request.",
                "Registry": registry}

    return response


def ufm_getmask_action(payload):
    global g_ufmlog

    if payload["MaskType"] == "All":
        error_mask = int(g_ufmlog.ufmlog.get_log_error())
        warning_mask = int(g_ufmlog.ufmlog.get_log_warning())
        info_mask = int(g_ufmlog.ufmlog.get_log_info())
        debug_mask = int(g_ufmlog.ufmlog.get_log_debug())
        detail_mask = int(g_ufmlog.ufmlog.get_log_detail())

        response = {"Status": 200,
                    "Message": "Successfully completed request.",
                    "ErrorMask": error_mask,
                    "WarningMask": warning_mask,
                    "InfoMask": info_mask,
                    "DebugMask": debug_mask,
                    "DetailMask": detail_mask}

    elif payload["MaskType"] == "ErrorMask":
        error_mask = g_ufmlog.ufmlog.get_log_error()
        response = {"Status": 200,
                    "Message": "Successfully completed request.",
                    "ErrorMask": error_mask}

    elif payload["MaskType"] == "WarningMask":
        warning_mask = g_ufmlog.ufmlog.get_log_warning()
        response = {"Status": 200,
                    "Message": "Successfully completed request.",
                    "WarningMask": warning_mask}

    elif payload["MaskType"] == "InfoMask":
        info_mask = g_ufmlog.ufmlog.get_log_info()
        response = {"Status": 200,
                    "Message": "Successfully completed request.",
                    "InfoMask": info_mask}

    elif payload["MaskType"] == "DebugMask":
        debug_mask = g_ufmlog.ufmlog.get_log_debug()
        response = {"Status": 200,
                    "Message": "Successfully completed request.",
                    "DebugMask": debug_mask}

    elif payload["MaskType"] == "DetailMask":
        detail_mask = g_ufmlog.ufmlog.get_log_detail()
        response = {"Status": 200,
                    "Message": "Successfully completed request.",
                    "DetailMask": detail_mask}

    return response


def ufm_setmask_action(payload):
    global g_ufmlog

    response = {"Status": 200,
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

        response = {"Status": 200,
                    "Message": "Successfully requested shutdown."}
    elif type == 'ForceRestart':
        g_ufmlog.info("RESTART: requested.")

        cmd = "python ufm.py restart &"
        subprocess.call(cmd, shell=True)

        response = {"Status": 200,
                    "Message": "Successfully requested restart."}
    else:
        response = {"Status": 400,
                    "Message": "Bad Request"}

    return response
