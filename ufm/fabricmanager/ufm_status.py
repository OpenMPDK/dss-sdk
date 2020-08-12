import socket
from enum import Enum
from subprocess import PIPE, Popen
from common.ufmdb import ufmdb
from common.ufmlog import ufmlog
from ufm_thread import UfmThread


class UfmHealthStatus(Enum):
    HEALTHY = 1
    NOT_HEALTHY = 2


class UfmLeaderStatus(Enum):
    LEADER = 1
    NOT_LEADER = 2


class UfmStatus(UfmThread):
    def __init__(self, onHealthChangeCb=None, onHealthChangeCbArgs=None,
                 onLeaderChangeCb=None, onLeaderChangeCbArgs=None):
        self.is_healthy = False
        self.is_leader = False

        self.onHealthChangeCb = onHealthChangeCb
        self.onHealthChangeCbArgs = onHealthChangeCbArgs
        self.onLeaderChangeCb = onLeaderChangeCb
        self.onLeaderChangeCbArgs = onLeaderChangeCbArgs

        self.dbclient = ufmdb.client(db_type='etcd')
        super(UfmStatus, self).__init__()

    def __del__(self):
        self.stop()

    def isHealthy(self):
        return self.is_healthy

    def isLeader(self):
        return self.is_leader

    def __updateHealthy(self):
        cmd = "ETCDCTL_API=2 etcdctl cluster-health"
        pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
        out, err = pipe.communicate()
        if out:
            for line in out.decode('utf-8').splitlines():
                if 'cluster is healthy' in line:
                    self.__setHealthy()
                    return

        self.__setHealthy(False)

    def __updateLeader(self):
        status = self.dbclient.status()
        leadername = status.leader.name.lower()
        leadername = leadername.split('.')[0]
        hostname = socket.gethostname().lower()
        hostname = hostname.split('.')[0]
        self.__setLeader((hostname == leadername or leadername == "default"))

    def __monitor(self, cbArgs):
        self.__updateHealthy()
        self.__updateLeader()

    def __setHealthy(self, is_healthy=True):
        is_healthy_prev = self.is_healthy
        self.is_healthy = is_healthy

        if is_healthy_prev != self.is_healthy and self.onHealthChangeCb is not None:
            self.onHealthChangeCb(UfmHealthStatus.HEALTHY if self.is_healthy else UfmHealthStatus.NOT_HEALTHY, self.onHealthChangeCbArgs)

    def __setLeader(self, is_leader=True):
        is_leader_prev = self.is_leader
        self.is_leader = is_leader

        if is_leader_prev != self.is_leader and self.onLeaderChangeCb is not None:
            self.onLeaderChangeCb(UfmLeaderStatus.LEADER if self.is_leader else UfmLeaderStatus.NOT_LEADER, self.onLeaderChangeCbArgs)

    def start(self):
        super(UfmStatus, self).start(threadName='UfmStatus', cb=self.__monitor, cbArgs=self, repeatIntervalSecs=2.0)

    def stop(self):
        super(UfmStatus, self).stop()
        self.onLeaderChangeCb(UfmLeaderStatus.NOT_LEADER, self.onLeaderChangeCbArgs)
