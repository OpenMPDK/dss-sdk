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


from os.path import join

SUCCESS = 200
SERVER_ERROR = 500
SERVER_ERROR_STR = 'Internal Server Error'
NOT_FOUND = 404
NOT_FOUND_STR = 'Not Found'
METHOD_NOT_ALLOWED = 405
METHOD_NOT_ALLOWED_STR = 'Method Not Allowed'

# Base URL of REST APIs
REST_BASE = '/redfish/v1'
# NAC constants
SYSTEM = 'System'
SYSTEMS = 'Systems'
SYSTEMS_URL = join(REST_BASE, SYSTEMS)
ETH_INTERFACES = 'EthernetInterfaces'

STORAGE = 'Storage'
DRIVES = 'Drives'

FABRIC = 'Fabric'
FABRICS = 'Fabrics'

SWITCH = 'Switch'
SWITCHES = 'Switches'

PORT = 'Port'
PORTS = 'Ports'

VLAN = 'VLAN'
VLANS = 'VLANs'

ODATA_ID = '@odata.id'
STATUS = 'Status'
HEALTH = 'Health'
STATE = 'State'
STATUS_OK = 'OK'
IDENTIFIERS = 'Identifiers'
DURABLENAME = 'DurableName'
SERVERNAME = 'ServerName'
OEM = 'oem'
