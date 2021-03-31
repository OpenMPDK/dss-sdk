"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import platform
from backend_layer import BackendLayer
from events.events import save_events_to_etcd_db, get_events_from_etcd_db
from log_setup import get_logger


class  TargetEvents:
    def __init__(self,endpoint="127.0.0.1", port=23790, server_base_key_prefix=""):
        self.TARGET_STATUS_PREFIX = server_base_key_prefix + "/target/status/" + platform.node().split('.', 1)[0] + "/"
        self.backend = BackendLayer(endpoint, port)
        self.logger = get_logger()

    def store_events(self,events):
        """
        Store events to the ETCD database
        :param events: <list> list of events
        :return: <bool> True/False on success and failure.
        """
        self.logger.info("{} Target events changed ...".format(len(events)))
        try:
            save_events_to_etcd_db(self.backend.db_handle, events)
        except Exception as e:
            self.logger.exception("Store Events: {}".format(str(e)))
            return False
        return True

    def get_events(self):
        """
        Construct the message if not done.
        :param message: <string> json string message
        :return: <dict> event - {"EVENT_NAME": "ARGS"} -> {"TARGET_IO_BLOCKING": {"DISK_SLNO": "xxyyy233", "STATUS": "DOWN" "MESSAGE": "IO FAILURE"}}
        """
        target_node = platform.node().split('.', 1)[0]
        events = get_events_from_etcd_db(self.backend.db_handle)
        target_events = {}
        for event in events:
            if event.get("name", False) and not target_events:
                target_events[event["name"]] = [event["args"]]
            else:
                target_events[event["name"]].append(event["args"])
        return target_events

    def read_target_status(self):
        """
        Get target status from ETCD database.
        :return: <dict>  Return target status
        """
        status = {}
        try:
            status = self.backend.get_json_prefix(self.TARGET_STATUS_PREFIX)
        except Exception as e:
            self.logger.exception(str(e))
        return status

    def write_target_status(self, status):
        """
        Store target status into ETCD database.
        :return: <bool> True/False, on success and failure respectively.
        """
        try:
            self.backend.write_dict_to_etcd(status, self.TARGET_STATUS_PREFIX)
        except Exception as e:
            self.logger.exception(str(e))
            return False
        return True

    def compare_target_status(self, cached_status={}, status={}):
        """
        - Compare the recent target status with cached target status.
        - Changes on status raises events.
        - Store the events into ETCD database and update status into ETCD.
        :param cached_status:<dict>
        :param status:<dict>  Recent target status
        :return:<bool> True if event exist else False.
        """
        events = []
        self.logger.info("Comparing target status")
        try:
            timestamp = status.pop("timestamp", None )
            for event_name, event_status in status.items():
                for key, value in event_status.items():
                    if (event_name not in cached_status or
                        key not in cached_status[event_name] or
                        cached_status[event_name][key]["status"] != value[
                                "status"]):
                        value["slno"] = key
                        event = {"node": platform.node().split('.', 1)[0],
                                 "name": event_name,
                                 "args": value,
                                 "timestamp": timestamp
                                }
                        events.append(event)
        except Exception as e:
            self.logger.error(e)

        # check if there is any events then send them to ETCD, Update status to cache as well as into ETCD.
        if events:
            self.store_events(events)
            self.write_target_status(status)
            return True

        return False

