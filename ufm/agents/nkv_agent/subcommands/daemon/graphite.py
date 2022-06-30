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
import netaddr
import pickle
import socket
import struct
import time
import re

from utils.log_setup import agent_logger


class CarbonMessageManager:
    def __init__(self, nvmf_pid,
                 carbon_server, carbon_port):
        self.logger = agent_logger

        self.nvmf_pid = nvmf_pid

        # Statistics DB Connection information
        self.ip = carbon_server
        self.port = int(carbon_port)
        self.stats_q = collections.deque([], maxlen=512)

        self.sock = socket.socket()
        self.COUNTER_MAX = 0xffffffffffffffff
        self.stop_flag = 0

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
                if netaddr.valid_ipv4(self.ip):
                    self.sock = socket.socket()
                    self.sock.connect((self.ip, self.port))
                elif netaddr.valid_ipv6(self.ip):
                    self.sock = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, 0)
                    self.sock.connect((self.ip, self.port, 0, 0))
                else:
                    self.logger.error('Invalid IP provided %s', self.ip)
                    time.sleep(30)
                    continue
                self.logger.info("Connection established")
                break
            except Exception as e:
                self.logger.error("Failed to establish connection, retrying.. {}".format(e))
                time.sleep(5)

    def push_statistics(self, stopper_event, poll_interval=1):

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


class StatsdManager:
    def __init__(self, nvmf_pid,
                 statsd_server, statsd_port):
        self.logger = agent_logger
        self.nvmf_pid = nvmf_pid
        # StatsD Connection information
        self.ip = statsd_server
        self.port = int(statsd_port)
        self.stats_q = collections.deque([], maxlen=512)
        # As per the information on
        # https://github.com/etsy/statsd/blob/master/docs/metric_types.md,
        # Maximum size 1432 for MTU 1500
        # 8932 for MTU 9000
        self.max_size = 1432
        self.stop_flag = 0
        self.COUNTER_MAX = 0xffffffffffffffff

        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        except:
            self.logger.exception('Exception in creating the StatsD client '
                                  'connection')

    def check_statsd_server_status(self):
        status = False
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2)
            self.logger.info("Establishing connection to '%s:%d'" %
                             (self.ip, self.port))
            sock.connect((self.ip, self.port))
            sock.send('health')
            data = sock.recv(512)
            sock.close()
            if data and 'up' in data:
                status = True
        except:
            self.logger.exception('Exception in checking StatsD server status')

        return status

    def connect_with_retry(self):
        while True:
            status = self.check_statsd_server_status()
            if status:
                self.logger.info('StatsD server is UP')
                break
            self.logger.info('Retrying connection to StatsD server...')
            time.sleep(5)

    def submit_message(self, tuples):
        if self.stop_flag:
            self.logger.info("Graphite Instance is getting stopped. Not "
                             "accepting any more messages")
            return

        self.stats_q.append(tuples)

    def send_data(self, message):
        try:
            out = self.socket.sendto(message, (self.ip, self.port))
            if out != len(message):
                self.logger.info(
                    'Sent data smaller than payload - sent[%s] expected ['
                    '%s]', out, len(message))
        except:
            self.logger.exception(
                'Exception in sending message to StatsD server %s', self.ip)
            self.connect_with_retry()

    def push_statistics(self, stopper_event, poll_interval=1):
        while not stopper_event.is_set():
            if self.stats_q:
                tuples = self.stats_q.popleft()
                message = ""
                msg_len = 0
                for t in tuples:
                    entry = t[0] + ":" + t[1][1] + "|g\n"
                    entry_len = len(entry)
                    if msg_len + entry_len > self.max_size:
                        self.send_data(message)
                        message = entry
                        msg_len = entry_len
                    else:
                        message = message + entry
                        msg_len += entry_len
                if msg_len:
                    self.send_data(message)
            else:
                time.sleep(poll_interval)

    def set_address(self, new_statsd_server, new_statsd_port):
        # Allows etcd override by OSMDaemon class to connect to another
        # StatsD server
        self.ip = new_statsd_server
        self.port = int(new_statsd_port)
        self.logger.info('StatsD new IP and port updated')
        return self.ip, self.port

    def close(self):
        # Make sure this is called without data getting added
        # Otherwise it goes into blocking loop
        self.stop_flag = 1
        while self.stats_q:
            tuples = self.stats_q.popleft()
            message = ""
            msg_len = 0
            for t in tuples:
                entry = t[0] + ":" + t[1][1] + "|g\n"
                entry_len = len(entry)
                if msg_len + entry_len > self.max_size:
                    self.send_data(message)
                    message = entry
                    msg_len = entry_len
                else:
                    message = message + entry
                    msg_len += entry_len
            if msg_len:
                self.send_data(message)
        self.socket.close()
