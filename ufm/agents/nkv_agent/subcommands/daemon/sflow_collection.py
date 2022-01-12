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
import json
import logging
import logging.config
import pexpect
import pickle
# import subprocess
import re
import socket
import struct
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
    logger = logging.getLogger('sflowcollector')
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
        pattern = "(127)\.((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\.)+"
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


class SFlowStatsCollector(object):
    def __init__(self, cluster_id, target_id, statsdb_obj,
                 logger, metrics_blacklist_regex=None, sflowtool_path='/usr/bin/sflowtool'):
        """
        :param cluster_id: Cluster ID
        :param target_id:  Target ID
        :param statsdb_obj: Metrics DB object instance to push the stats as a message which will
                          eventually push to metrics DB
        """
        self.event = threading.Event()
        self.cluster_id = cluster_id
        self.target_id = target_id
        self.statsdb_obj = statsdb_obj
        self.sflowtool_path = sflowtool_path
        self.metrics_blacklist_regex = metrics_blacklist_regex
        self.log = logger

    def poll_statistics(self, stopper_event):
        cmd = self.sflowtool_path + ' -j '
        try:
            proc = pexpect.spawn(cmd, timeout=None)
        except Exception:
            self.log.exception('Caught exception while running sflowtool')
            return

        while not proc.eof() and not stopper_event.is_set():
            line = proc.readline()
            line = line.strip()
            if line:
                self.log.debug('OUT [%s]', line)
                tuples = []
                datagram = json.loads(line)
                timestamp = datagram['unixSecondsUTC']
                # Change agent_ip from (.) to (_) for graphite storage purpose
                switch_ip = datagram['datagramSourceIP']
                agent_id = datagram['agent']  # .replace('.', '_')
                samples = datagram['samples']
                for sample in samples:
                    if sample['sampleType'] == 'COUNTERSSAMPLE':
                        # Change source_id (0:24) to 0_24 for graphite storage purpose
                        source_id = sample['sourceId']  # .replace(':', '_')
                        elements = sample['elements']
                        for element in elements:
                            block_tag = element.pop('counterBlock_tag')
                            if block_tag != '0:1':
                                continue
                            if_index = element.pop('ifIndex')
                            for k, v in element.items():
                                metric_path = "sflow_metrics.%s;switch_ip=%s;agent_id=%s;source_id=%s;if_index=%s;type=sflow" % \
                                              (k, switch_ip, agent_id, source_id, if_index)
                                if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                                    continue
                                tuples.append((metric_path, (int(timestamp), int(v))))

                    elif sample['sampleType'] == 'FLOWSAMPLE':
                        # Change source_id (0:24) to 0_24 for graphite storage purpose
                        source_id = sample['sourceId']  # .replace(':', '_')
                        elements = sample['elements']
                        iface_port = sample['inputPort']
                        for e in elements:
                            if 'TCPSrcPort' in e:
                                proto = 'tcp'
                                src_port = e['TCPSrcPort']
                                dst_port = e['TCPDstPort']
                            else:
                                proto = 'udp'
                                src_port = e['UDPSrcPort']
                                dst_port = e['UDPDstPort']

                            metric_path = ("sflow_metrics.sampled_packet_size;switch_ip=%s;agent_id=%s;source_id=%s;"
                                           "if_index=%s;src_mac=%s;dst_mac=%s;src_ip=%s;dst_ip=%s;vlan=%s;"
                                           "priority=%s;%s_src_port=%s;%s_dst_port=%s;type=sflow") % \
                                          (switch_ip, agent_id, source_id, iface_port, e['srcMAC'], e['dstMAC'],
                                           e['srcIP'], e['dstIP'], e['decodedVLAN'], e['decodedPriority'],
                                           proto, src_port, proto, dst_port)
                            if self.metrics_blacklist_regex and self.metrics_blacklist_regex.match(metric_path):
                                continue
                            tuples.append((metric_path, (int(timestamp), int(e['sampledPacketSize']))))

                if tuples:
                    self.log.debug('Data sent to DB [%s]', str(tuples))
                    self.statsdb_obj.submit_message(tuples)

        try:
            ret = proc.terminate()
            if ret:
                self.log.info('sflowtool process terminated successfully')
            else:
                self.log.error('sflowtool process termination failed')
        except Exception:
            self.log.exception('sflowtool process termination exception')

        self.log.info('sFlow data Poll statistics thread exited')

    def run_stats_collector(self):
        self.event.clear()
        self.log.info('Starting sFlow stats collection thread')
        thr = threading.Thread(target=self.poll_statistics, args=[self.event])
        thr.start()
        self.log.info('Started sFlow stats collection thread')

    def stop_stats_collector(self):
        self.log.info('Stopping the sFlow stats collection')
        self.event.set()


if __name__ == '__main__':
    log = _set_logger('/var/log/sflowcollector.log')
    statsdb_obj = _GraphiteDBManager('10.1.30.142', 2004, log)
    statsdb_obj.start_push_stats_thread()
    sflow_statsdb_obj = SFlowStatsCollector('123', '456', statsdb_obj, log)
    sflow_statsdb_obj.run_stats_collector()
    time.sleep(720)
    sflow_statsdb_obj.stop_stats_collector()
    time.sleep(5)
    statsdb_obj.stop_push_stats_thread()
