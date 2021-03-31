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

import os
from datetime import datetime
import threading
import time
from subprocess import PIPE, Popen

from ufm_thread import UfmThread
from systems.ufm import ufm_constants
from systems.ufm_message import Subscriber


class UfmPoller(UfmThread):
    def __init__(self, ufmArg=None):
        self.ufmArg = ufmArg
        self.startTime = 0
        self._running = False

        self.event = threading.Event()
        self.msgListner = Subscriber(event=self.event,
                                     ports=(self.ufmArg.ufmPorts),
                                     topics=('poller',))

        super(UfmPoller, self).__init__()
        self.ufmArg.log.info("Init {}".format(self.__class__.__name__))

    def __del__(self):
        self.ufmArg.log.info("Del {}".format(self.__class__.__name__))
        # self.stop()
        pass

    def start(self):
        self.ufmArg.log.info("Start {}".format(self.__class__.__name__))
        self.ufmArg.lastDatabaseIsUp = False
        self.startTime = datetime.now()
        self.ufmArg.db.put(ufm_constants.UFM_UPTIME_KEY, str(0))
        self.ufmArg.db.put('/cluster/uptime_in_seconds', str(0))

        self.msgListner.start()

        self._running = True
        super(UfmPoller, self).start(threadName='UfmPoller', cb=self._poller,
                                     cbArgs=self.ufmArg, repeatIntervalSecs=60.0)

    def stop(self):
        self.event.set()
        super(UfmPoller, self).stop()
        self.ufmArg.db.put('/cluster/uptime_in_seconds', str(0))
        self.msgListner.stop()
        self.msgListner.join()

        self._running = False
        self.ufmArg.log.info("Stop {}".format(self.__class__.__name__))

    def is_running(self):
        return self._running

    def read_system_uptime(self):
        uptime = 0
        with open('/proc/uptime') as f:
            out = f.read()
            # Convert seconds to hours
            uptime = int(float(out.split()[0]))/3600

        return uptime

    def read_node_capacity_in_kb(self):
        df = os.statvfs('/')
        if df.f_blocks > 0:
            return df.f_blocks * 4
        return 0

    def read_avail_space_percent():
        df_struct = os.statvfs('/')
        if df_struct.f_blocks > 0:
            return df_struct.f_bfree * 100 / df_struct.f_blocks
        return 0

    def isDatabaseUp(self):
        cmd = "ETCDCTL_API=3 etcdctl endpoint health"

        pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = pipe.communicate()
        if out:
            for line in out.decode('utf-8').splitlines():
                if 'healthy' in line:
                    return True

        return False

    def isDatabaseRunning(self, ufmArg):
        databaseIsUp = self.isDatabaseUp()

        if ufmArg.lastDatabaseIsUp != databaseIsUp:
            ufmArg.lastDatabaseIsUp = databaseIsUp
            statusKey = "/cluster/{}".format(ufmArg.hostname)
            if databaseIsUp:
                databaseState = 'up'
            else:
                databaseState = 'down'
                # if database is down, do not try to write to it

                databaseStateMsg = {'module': 'UfmPoller',
                                    'service': 'poller',
                                    'database_state': 'down'}

                ufmArg.publisher.send(ufm_constants.UFM_DATABASE_STATE, databaseStateMsg)

                return False

            ufmArg.db.put(statusKey + "/status", databaseState)
            ufmArg.db.put(statusKey + "/status_updated", str(int(time.time())))

        try:
            if not ufmArg.nodeStatusLease or ufmArg.nodeStatusLease.remaining_ttl < 0:
                ufmArg.nodeStatusLease = ufmArg.db.lease(20)
            else:
                ufmArg.nodeStatusLease.refresh_lease()
        except Exception:
            ufmArg.log.exception('Failed to creating/renewing lease')

        # Only leader and status have a lease
        try:
            dbStatus = ufmArg.db.status()

            ufmArg.db.put("/cluster/leader", dbStatus.leader.name.lower(), lease=ufmArg.nodeStatusLease)
        except Exception as ex:
            ufmArg.log.error("Failed to update leader-name and database size: {}".format(ex))
            return False

        return True

    def _poller(self, ufmArg):
        if not self.isDatabaseRunning(ufmArg):
            ufmArg.log.error("Database is down")
            return

        # Save current uptime to DB
        uptimeString = str((datetime.now() - self.startTime).seconds)

        ufmArg.db.put(ufm_constants.UFM_UPTIME_KEY, uptimeString)
        ufmArg.db.put('/cluster/uptime_in_seconds', uptimeString)

        hostname = ufmArg.hostname
        uptime = self.read_system_uptime()
        ufmArg.db.put("/cluster/{}/uptime".format(hostname), str(uptime))

        node_capacity_Kb = self.read_node_capacity_in_kb()
        if node_capacity_Kb:
            ufmArg.db.put("/cluster/{}/total_capacity_in_kb".format(hostname), str(node_capacity_Kb))

        avail_space_percent = self.read_avail_space_percent()
        if avail_space_percent:
            ufmArg.db.put(ufm_constants.UFM_LOCAL_DISKSPACE, str(avail_space_percent))

            # NKV needs the disk space in this location of the db
            ufmArg.db.put("/cluster/{}/space_avail_percent".format(hostname), str(avail_space_percent))

        if avail_space_percent > 95.0:
            diskSpaceMsg = dict()
            diskSpaceMsg['status'] = "ok"
            diskSpaceMsg['service'] = "UfmPoller"
            diskSpaceMsg['local_disk_space'] = int(avail_space_percent)

            self.ufmArg.publisher.send(ufm_constants.UFM_LOCAL_DISKSPACE, diskSpaceMsg)
