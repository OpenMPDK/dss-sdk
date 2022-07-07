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


import collections
import logging
import logging.config
import os
import pickle
import re
import socket
import struct
# import subprocess
import threading
import time


def _set_logger(log_filename):
    log_dict = {
        'version': 1.0,
        'disable_existing_loggers': True,
        'formatters': {
            'simple': {
                'format': '%(asctime)s [%(levelname)s] %(name)s - %(pathname)s [%(lineno)d] %(message)s',
                'datefmt': '%Y-%m-%d %H:%M:%S'
            }
        },
        'handlers': {
            'file': {
                'class': 'logging.FileHandler',
                'level': 'DEBUG',
                'formatter': 'simple',
                'filename': log_filename,
                'encoding': 'utf-8'
            },
        },
        'root': {
            'level': 'DEBUG',
            'handlers': ['file']
        }
    }

    logging.config.dictConfig(log_dict)
    logger = logging.getLogger('mlnx_rdma_counters')
    return logger


class _GraphiteDBManager:
    def __init__(self, carbon_server, carbon_port, logger):
        self.logger = logger
        # Statistics DB Connection information
        self.ip = carbon_server
        self.port = int(carbon_port)
        self.stats_q = collections.deque([], maxlen=512)

        self.sock = socket.socket()
        self.COUNTER_MAX = 0xffffffffffffffff
        self.stop_flag = 0
        self.event = threading.Event()

    def submit_message(self, tuples):
        if self.stop_flag:
            self.logger.info("Graphite Instance is getting stopped. Not "
                             "accepting any more messages")
            return
        payload = pickle.dumps(tuples, protocol=2)
        header = struct.pack("!L", len(payload))
        message = header + payload
        self.stats_q.append(message)

    @staticmethod
    def detect_and_pad(data):
        missing_padding = len(data) % 4
        if missing_padding:
            padded_data = data + (b'=' * (4 - missing_padding))
            return padded_data
        else:
            return data

    def connect_with_retry(self, stopper_event):
        while True and not stopper_event.is_set():
            try:
                self.logger.info("Establishing connection to '%s:%d'" %
                                 (self.ip, self.port))
                if self.sock:
                    self.sock.close()
                self.sock = socket.socket()
                self.sock.connect((self.ip, self.port))
                self.logger.info("Connection established")
                break
            except Exception as e:
                self.logger.error("Failed to establish connection, retrying.. {}".format(e))
                time.sleep(5)

    def push_statistics(self, stopper_event, poll_interval=10):

        # Pattern to match any combination of 127.xxx.xxx.xxx
        pattern = r"(127)\.((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\.)+"
        regex = re.compile(pattern)

        while not stopper_event.is_set():
            if self.ip == '0.0.0.0' or regex.match(self.ip):
                time.sleep(5)
                continue

            if self.stats_q:
                message = self.stats_q.popleft()
                while True:
                    try:
                        self.sock.sendall(message)
                        break
                    except:
                        self.connect_with_retry(stopper_event)
            else:
                self.logger.debug('StatsQ is empty. Waiting for data')
                time.sleep(poll_interval)

    def set_address(self, new_carbon_server, new_carbon_port):
        # Allows etcd override by OSMDaemon class to connect to
        # another carbon server
        self.ip = new_carbon_server
        self.port = int(new_carbon_port)
        if self.sock:
            self.sock.close()
        self.sock = socket.socket()
        return self.ip, self.port

    def close(self):
        # Make sure this is called without data getting added
        # Otherwise it goes into blocking loop
        self.stop_flag = 1
        while self.stats_q:
            message = self.stats_q.popleft()
            while True:
                try:
                    self.sock.sendall(message)
                    break
                except:
                    self.connect_with_retry()
        self.sock.close()

    def start_push_stats_thread(self):
        self.event.clear()
        self.logger.info('Starting DB push stats thread')
        thr = threading.Thread(target=self.push_statistics, args=[self.event])
        thr.start()
        self.logger.info('Started DB push stats thread')

    def stop_push_stats_thread(self):
        self.logger.info('Stopping the DB push stats thread')
        self.event.set()
        self.logger.info('Stopped DB push stats thread')


class MellanoxRDMAStats(object):
    def __init__(self, cluster_id, target_id, statsdb_obj, logger, metrics_blacklist_regex=None):
        self.cluster_id = cluster_id
        self.target_id = target_id
        self.statsdb_obj = statsdb_obj
        self.metrics_blacklist_regex = metrics_blacklist_regex
        self.log = logger
        self.event = threading.Event()

    def poll_rdma_stats(self, sec):
        sysfs_mlnx_dir = '/sys/class/infiniband/'
        hw_counters_dir = '/ports/1/hw_counters'
        tuples = []
        while not self.event.is_set():
            for _, dirs, _ in os.walk(sysfs_mlnx_dir):
                for d in dirs:
                    root_dir = sysfs_mlnx_dir + d + hw_counters_dir
                    for _, _, files in os.walk(root_dir):
                        for f in files:
                            filename = os.path.join(root_dir, f)
                            with open(filename) as fh:
                                val = fh.readline().strip()
                                metric_path = ("network.%s;cluster_id=%s;target_id=%s;type=mlnx" %
                                               (f, self.cluster_id, self.target_id))
                                if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                                    continue
                                timestamp = time.time()
                                tuples.append((metric_path, (timestamp, val)))
            if tuples:
                self.log.debug('Metrics sent to DB %s', str(tuples))

                if self.statsdb_obj:
                    self.statsdb_obj.submit_message(tuples)

            time.sleep(sec)

        self.log.info('RDMA Poll statistics thread exited')

    def run_stats_collector(self, interval=20):
        self.event.clear()
        self.log.info('Starting RDMA counters stats collection thread')
        thr = threading.Thread(target=self.poll_rdma_stats, args=[interval])
        thr.start()
        self.log.info('Started RDMA counters stats collection thread')

    def stop_stats_collector(self):
        self.log.info('Stopping the RDMA stats collection')
        self.event.set()


if __name__ == '__main__':
    log = _set_logger('/var/log/sflowcollector.log')
    statsdb_obj = _GraphiteDBManager('10.1.30.142', 2004, log)
    statsdb_obj.start_push_stats_thread()
    nso = MellanoxRDMAStats('111', '222', statsdb_obj, log)
    nso.poll_rdma_stats(5)
