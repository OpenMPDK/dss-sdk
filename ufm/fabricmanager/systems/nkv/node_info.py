import time
import threading
from datetime import datetime
from subprocess import PIPE, Popen

LEASE_INTERVAL = 20


class Node_Info(threading.Thread):
    def __init__(self, stopper_event=None, db=None, hostname=None,
                 check_interval=600, log=None):
        super(Node_Info, self).__init__()
        self.stopper_event = stopper_event
        self.db = db
        self.hostname = hostname
        self.check_interval = check_interval
        self.log = log
        self.node_status_lease = None
        self.last_db_status = None
        self.log.info("Init {}".format(self.__class__.__name__))

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
        self.log.info("Start Update Node Info thread")

        while not self.stopper_event.is_set():
            self.db.put('/cluster/uptime_in_seconds', str((datetime.now() - self.start_time).seconds))

            db_status = self.read_db_status()
            if self.last_db_status != db_status:
                self.last_db_status = db_status
                self.db.put("/cluster/{}/status".format(self.hostname), db_status)

            self.db.put("/cluster/{}/status_updated".format(self.hostname), str(int(time.time())))

            uptime = self.read_system_uptime()
            self.db.put("/cluster/{}/uptime".format(self.hostname), str(uptime))

            try:
                if not self.node_status_lease or self.node_status_lease.remaining_ttl < 0:
                    self.node_status_lease = self.db.lease(LEASE_INTERVAL)
                else:
                    # _, lease_id = self.db.refresh_lease(lease=self.node_status_lease)
                    self.node_status_lease.refresh_lease()
                    self.log.error("Lease refreshed")
            except Exception:
                self.log.exception('Failed to creating/renewing lease')
                continue  # This can be changed to a break later

            # Only leader and status have a lease
            try:
                db_status = self.db.status()

                self.db.put("/cluster/leader",
                            db_status.leader.name.lower(), lease=self.node_status_lease)

                # self.db.put("/cluster/{}/db_size".format(self.hostname),
                #            str(db_status.dbSize), lease=self.node_status_lease)
            except Exception as ex:
                self.log.error("Failed to update leader name and database size")
                self.log.error("Exception: {} {}".format(__file__, ex))

            self.stopper_event.wait(self.check_interval)

        self.db.put('/cluster/uptime_in_seconds', str(0))
        self.log.info("Update Node Info thread has Stopped")
