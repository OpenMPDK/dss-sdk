
import time
import threading
from datetime import datetime

from subprocess import PIPE, Popen

LEASE_INTERVAL = 20


class Node_Info(threading.Thread):
    def __init__(self,
                 stopper_event=None,
                 db=None,
                 hostname=None,
                 check_interval=600,
                 logger=None):
        super(Node_Info, self).__init__()
        self.stopper_event = stopper_event
        self.db = db
        self.hostname = hostname
        self.check_interval = check_interval
        self.logger = logger
        self.node_status_lease = None
        self.last_db_status = None

    def read_system_uptime(self):
        uptime = 0
        with open('/proc/uptime') as f:
            out = f.read()
            # Convert seconds to hours
            uptime = int(float(out.split()[0]))/3600

        return uptime

    def read_db_status(self):
        cmd = "ETCDCTL_API=3 etcdctl endpoint health"

        pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = pipe.communicate()
        if out:
            for line in out.decode('utf-8').splitlines():
                if 'healthy' in line:
                    return "up"

        return "down"

    def run(self):
        self.start_time = datetime.now()
        self.logger.debug("Start Update Node Info thread")

        while not self.stopper_event.is_set():
            try:
                if not self.node_status_lease or \
                                    self.node_status_lease.remaining_ttl < 0:
                    self.node_status_lease = self.db.create_lease(
                                                                LEASE_INTERVAL)
                else:
                    _, lease_id = self.db.refresh_lease(
                                                lease=self.node_status_lease)

                    if lease_id is None:
                        self.logger.error("Lease %s not refreshed" %
                                          self.node_status_lease.id)
            except Exception:
                self.logger.exception('Failed to creating/renewing lease')
                return

            db_status = self.read_db_status()

            try:
                status_obj = self.db.get_status()
            except Exception:
                status_obj = None

            self.db.save_key_value('/cluster/uptime_in_seconds',
                                   (datetime.now() - self.start_time).seconds)

            kv_dict = dict()
            kv_dict['/cluster/' + self.hostname + '/uptime'] = str(
                                                    self.read_system_uptime())

            if self.last_db_status != db_status:
                self.last_db_status = db_status
                kv_dict['/cluster/' + self.hostname + '/status'] = db_status

            kv_dict['/cluster/' + self.hostname + '/status_updated'] = str(
                                                            int(time.time()))

            strings_with_lease = ['/cluster/leader',
                                  '/cluster/' + self.hostname + '/status'
                                  ]
            if status_obj:
                kv_dict['/cluster/' + self.hostname + '/db_size'] = str(
                                                            status_obj.db_size)

                kv_dict['/cluster/leader'] = str(status_obj.leader.name)

            try:
                self.db.save_multiple_key_values(kv_dict, strings_with_lease,
                                                 self.node_status_lease)
            except Exception:
                self.logger.exception('Failed to save multiple KVs')

            self.stopper_event.wait(self.check_interval)

        self.db.save_key_value('/cluster/uptime_in_seconds', 0)
        self.logger.info("Update Node Info thread has Stopped")
