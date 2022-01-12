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


import logging
import copy
import traceback


'''
UFM Module bit masks
    - Simply assign a bit for a unique grouping of module log messages
'''
UFM_MAIN       = 0x00000001
UFM_DB         = 0x00000010
UFM_REDFISH_DB = 0x00000020
UFM_MQ         = 0x00000040
UFM_MONITOR    = 0x00010000
UFM_CONTROLLER = 0x00020000
UFM_COLLECTOR  = 0x00040000
UFM_POLLER     = 0x00080000
UFM_NKV        = 0x00100000
UFM_SWITCH     = 0x00200000

# All modules defined above must also be registered below
UfmModuleRegistry = { "UFM_MAIN":UFM_MAIN,
    "UFM_DB": UFM_DB,
    "UFM_REDFISH_DB": UFM_REDFISH_DB,
    "UFM_MQ": UFM_MQ,
    "UFM_MONITOR": UFM_MONITOR,
    "UFM_CONTROLLER": UFM_CONTROLLER,
    "UFM_COLLECTOR": UFM_COLLECTOR,
    "UFM_POLLER": UFM_POLLER,
    "UFM_NKV": UFM_NKV,
    "UFM_SWITCH": UFM_SWITCH,
}


UFM_ALL        = 0xFFFFFFFF
UFM_NONE       = 0x00000000




# Defualt UFM Log file
UFM_LOG_FILE  = 'ufm.log'

import time
import threading

class UfmLogEntry(object):
    def __init__(self, id=0, module=0, msg="", type="INFO"):
        self.timestamp = time.time()
        self.id = id
        self.module = module
        self.msg = msg
        self.type = type


# Handles global managment of log
class Ufmlog(object):
    def __init__(self, filename=UFM_LOG_FILE, log_info=UFM_ALL, log_error=UFM_ALL,
            log_warning=UFM_ALL, log_debug=UFM_NONE, log_detail=UFM_NONE, max_entries=1000):
        self.filename = filename
        self.level = logging.DEBUG
        self.format = '%(asctime)s %(message)s'
        self.datefmt = '%Y/%m/%d %H:%M:%S' # This format can be sorted as text
        self.count = 0
        self.log_info_mask = log_info
        self.log_error_mask = log_error
        self.log_warning_mask = log_warning
        self.log_debug_mask = log_debug
        self.log_detail_mask = log_detail

        # internal log management
        self.max_entries = max_entries
        self.entries = []

        # fill entry log with empty entries
        for i in range(0, self.max_entries):
            self.entries.append(None)
            i = i # Prevents pylint from complaining

        self._entry_id = 0
        self._entry_lock = threading.Lock()
        self.entry_index = 0
        self._base_id = 0

    def __del__(self):
        self.count = 0

    def open(self):
        if self.count == 0:
            logging.basicConfig(filename=self.filename, level=0, format=self.format, datefmt=self.datefmt)
            logging.info('UFMLOG: Opened')
            self.count = 1
        else:
            self.count += 1

    def close(self):
        if self.count == 1:
            self.count = 0
        else:
            self.count -= 1

    def clear_log(self):
        with self._entry_lock:
            self._base_id = self._entry_id
            print("clear_log: %d %d" %  (self._entry_id, self._base_id))

    def get_entry_id(self):
        return self._entry_id

    def get_entries(self, id, count):
        with self._entry_lock:
            rsp = []

            if id==None and count==None:  # Get all available entries
                id = self._base_id
                count = min(self.max_entries, self._entry_id - id)

            elif id==None and count!=None:  # Get last count entries (count priority)
                if count == 0:
                    return rsp

                count = min(count, self.max_entries, self._entry_id - self._base_id)
                id = self._entry_id - count

            elif id!=None and count==None: # ALl entries from id up (id priority)
                if id >= self._entry_id:
                    return rsp

                id = min(max(id, self._base_id), self._entry_id-1)
                count = min(self.max_entries, self._entry_id - id)

            else: # id!=None and count!=None:  # best fit (id priority)
                if id >= self._entry_id:
                    return rsp

                if count == 0:
                    return rsp

                id = min(max(id, self._base_id), self._entry_id-1)
                count = min(count, self.max_entries, self._entry_id - id)

            # copy entries to separate list so they don't change later
            for i in range(id, id+count):
                entry = copy.copy(self.entries[(i % self.max_entries)])

                if entry == None:
                    continue

                rsp.append(entry)

            return rsp

    def _add(self, module, type, msg):
        with self._entry_lock:
            entry = UfmLogEntry(id=self._entry_id, module=module, type=type, msg=msg)
            self.entries[self.entry_index] = entry

            self._entry_id += 1
            self.entry_index = self._entry_id % self.max_entries
            return entry

    def log_info(self, module, msg):
        entry = self._add(module, "INFO", msg)
        logging.info(str(entry.id)+': '+module+': INFO: '+msg)
        return

    def log_error(self, module, msg):
        entry = self._add(module, "ERROR", msg)
        logging.info(str(entry.id)+': '+module+': ERROR: '+msg)
        return

    def log_warning(self, module, msg):
        entry = self._add(module, "WARNING", msg)
        logging.info(str(entry.id)+': '+module+': WARNING: '+msg)
        return

    def log_debug(self, module, msg):
        entry = self._add(module, "DEBUG", msg)
        logging.info(str(entry.id)+': '+module+': DEBUG: '+msg)
        return

    def log_detail(self, module, msg):
        entry = self._add(module, "DETAIL", msg)
        logging.info(str(entry.id)+': '+module+': DETAIL: '+msg)
        return

    def log_except(self, module, excep):
        error_msg = traceback.format_exc()
        entry = self._add(module, "EXCEPTION", error_msg)
        logging.info(str(entry.id)+': '+module+': EXCEPTION: '+error_msg)
        #logging.exception(excep)
        return

    def set_log_info(self, mask):
        self.log_info_mask = mask

    def get_log_info(self):
        return (self.log_info_mask)

    def set_log_debug(self, mask):
        self.log_debug_mask = mask

    def get_log_debug(self):
        return (self.log_debug_mask)

    def set_log_warning(self, mask):
        self.log_warning_mask = mask

    def get_log_warning(self):
        return (self.log_warning_mask)

    def set_log_error(self, mask):
        self.log_error_mask = mask

    def get_log_error(self):
        return (self.log_error_mask)

    def set_log_detail(self, mask):
        self.log_detail_mask = mask

    def get_log_detail(self):
        return (self.log_detail_mask)

    def get_module_registry(self):
        global UfmModuleRegistry
        return (UfmModuleRegistry)

# Global ufmlog object
ufmlog = Ufmlog()


# Module level managment of log
class log(object):
    def __init__(self, module='UFM', mask=int(UFM_MAIN)):
        self.module = module.upper()
        self.mask = int(mask)
        self.ufmlog = ufmlog
        ufmlog.open()
        self.detail('Logging Started.')

    def __del__(self):
        ufmlog.close()

    def info(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_info_mask) != 0:
            string = msg % args
            ufmlog.log_info(self.module, string)

    def detail(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_detail_mask) != 0:
            string = msg % args
            ufmlog.log_detail(self.module, string)

    def debug(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_debug_mask) != 0:
            string = msg % args
            ufmlog.log_debug(self.module, string)

    def warning(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_warning_mask) != 0:
            string = msg % args
            ufmlog.log_warning(self.module, string)

    def error(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_error_mask) != 0:
            string = msg % args
            ufmlog.log_error(self.module, string)

    def exception(self, excep):
        ufmlog.log_except(self.module, excep)


# INFO

    def set_log_info(self, mask):
        ufmlog.set_log_info(mask)

    def get_log_info(self):
        return (ufmlog.get_log_info())

    def log_info_on(self):
        ufmlog.log_info_mask = ufmlog.log_info_mask | self.mask

    def log_info_off(self):
        ufmlog.log_info_mask = ufmlog.log_info_mask & ~self.mask

# DEBUG

    def set_log_debug(self, mask):
        ufmlog.set_log_debug(mask)

    def get_log_debug(self):
        return (ufmlog.get_log_debug())

    def log_debug_on(self):
        ufmlog.log_debug_mask = ufmlog.log_debug_mask | self.mask

    def log_debug_off(self):
        ufmlog.log_debug_mask = ufmlog.log_debug_mask & ~self.mask

# WARNING

    def set_log_warning(self, mask):
        ufmlog.set_log_warning(mask)

    def get_log_warning(self):
        return (ufmlog.get_log_warning())

    def log_warning_on(self):
        ufmlog.log_warning_mask = ufmlog.log_warning_mask | self.mask

    def log_warning_off(self):
        ufmlog.log_warning_mask = ufmlog.log_warning_mask & ~self.mask

# ERROR

    def set_log_error(self, mask):
        ufmlog.set_log_error(mask)

    def get_log_error(self):
        return (ufmlog.get_log_error())

    def log_error_on(self):
        ufmlog.log_error_mask = ufmlog.log_error_mask | self.mask

    def log_error_off(self):
        ufmlog.log_error_mask = ufmlog.log_error_mask & ~self.mask

# DETAIL

    def set_log_detail(self, mask):
        ufmlog.set_log_detail(mask)

    def get_log_detail(self):
        return (ufmlog.get_log_detail())

    def log_detail_on(self):
        ufmlog.log_detail_mask = ufmlog.log_detail_mask | self.mask

    def log_detail_off(self):
        ufmlog.log_detail_mask = ufmlog.log_detail_mask & ~self.mask
