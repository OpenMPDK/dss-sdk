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


"""Logger module for Cluster Manager
It supports file logging, syslog, console, time rotation and size rotation
The configuration file is optional. If given, it should be in the following format
[logging]
log_dir = <log directory>
log_file = <log file name>
log_level = DEBUG/INFO/WARN/ERROR
file = enabled/disabled
console = enabled/disabled
console_level = DEBUG/INFO/WARN/ERROR
syslog = enabled/disabled
syslog_level = DEBUG/INFO/WARN/ERROR
syslog_facility = local0/user
syslog_address = (<Syslog server IP_address>, <Syslog server port>)
syslog_socket = udp/tcp
time_rotation = enabled/disabled
interval = <interval time>
interval_unit = S(seconds), M(minutes), H(hours), D(days)
size_rotation = enabled/disabled
file_size = <file size for rotation>
file_cnt = <number of files to be backed up> -> Applies for {time, size}_rotation schemes

file, time_rotation, size_rotation are mutually exclusive. Enable only one of them.
Otherwise, the messages will be logged multiple times. Files will be rotated depending
on their respective settings

If the configuration file is not given, then only the file and syslog are enabled.
The file is saved to /var/log/<module_name>.log.
The debug level is set to a default 'INFO'
"""

import ast
import ConfigParser
import errno
import logging
import logging.config
import os
import socket
import time

CM_LOGGER_VERSION = 1.0


class CMLogger(object):
    """Logger module for Cluster Manager
    It supports file logging, syslog, console, time rotation and size rotation
    """
    def __init__(self, module_name, config_file=None, section="logging",
                 default_log_dir="/var/log/nkv-agent"):
        self.cfg_file = config_file
        self.cfg = ConfigParser.ConfigParser()
        self.module_name = module_name
        self.version = CM_LOGGER_VERSION
        self.default_log_dir = default_log_dir
        if self.cfg_file and os.path.exists(self.cfg_file):
            try:
                self.cfg.read(self.cfg_file)
                self._section = section
            except Exception as e:
                print("ERROR - Reading configuration file [%s] failed with error %s" % (self.cfg_file, e.message))
                self.cfg = None
        else:
            # print("INFO - configuration file for module %s doesn't exist" % module_name)
            self.cfg = None

    def get_log_config_options(self):
        """Read the various log options for the logging"""
        log_cfg = {}
        if not self.cfg or not self._section:
            return log_cfg

        section = self._section
        if not self.cfg.has_section(section):
            print("Section %s is not present in the configuration file" % section)
            return log_cfg

        options = self.cfg.options(section)
        for option in options:
            log_cfg[option] = self.cfg.get(section, option)

        if 'log_dir' in log_cfg:
            dir_name = log_cfg['log_dir']
            if not os.path.exists(dir_name):
                try:
                    os.makedirs(dir_name, 0o755)
                except OSError as e:
                    if e.errno != errno.EEXIST:
                        print("Got exception while creating the log dir %s, exception %s" % (dir_name, e))
        else:
            try:
                os.makedirs(self.default_log_dir, 0o755)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    print("Got exception while creating the log dir /var/log/, exception %s" % e)

        return log_cfg

    def create_logger(self):
        """Creates logger with the module_name given at the time of class init
        If the log configuration file is not given, then the file logging will be done to
        /var/log/<module_name>.log and syslog is written to syslog file with the
        debugging level 'INFO'
        Returns a logger instance to use for logging messages.
        """
        log_cfg = self.get_log_config_options()
        if 'log_file' in log_cfg:
            log_filename = log_cfg['log_dir'] + log_cfg['log_file']
        else:
            try:
                os.makedirs(self.default_log_dir, 0o755)
            except OSError as e:
                if e.errno != errno.EEXIST:
                    print("Got exception while creating the log dir %s, "
                          "exception %s" % (self.default_log_dir, e))
                    raise(e)
            log_filename = os.path.join(self.default_log_dir, self.module_name + '.log')

        if 'syslog_address' in log_cfg:
            syslog_address = ast.literal_eval(log_cfg['syslog_address'])
        else:
            syslog_address = '/dev/log'

        if 'syslog_address' in log_cfg:
            if log_cfg['syslog_socket'] == 'tcp':
                syslog_socket = socket.SOCK_STREAM
            else:
                syslog_socket = socket.SOCK_DGRAM
        else:
            syslog_socket = None

        log_dict = {
            'version': self.version,
            'disable_existing_loggers': True,
            'formatters': {
                'simple': {
                    'format': '%(asctime)s [%(levelname)s] %(name)s - %(pathname)s [%(lineno)d] %(message)s',
                    'datefmt': '%Y-%m-%d %H:%M:%S'
                },
                'syslog': {
                    # 'format': '[%(levelname)s]  %(name)s -%(filename)s[%(
                    # lineno)d] %(message)s',
                    # 'datefmt': '%Y-%m-%d %H:%M:%S'
                    # %(name) is the tag for syslog and it should be in the
                    # second field as well. Otherwise the message wont be
                    # detected by the rsyslog to do special processing
                    'format': ('%(name)s %(name)s %(filename)s:%(lineno)d '
                               '%(levelname)s %(message)s'),
                }
            },
            'handlers': {
                'console': {
                    'class': 'logging.StreamHandler',
                    'level': 'console_level' in log_cfg and log_cfg['console_level'] or 'INFO',
                    'formatter': 'simple',
                    'stream': 'ext://sys.stdout'
                },
                'syslog': {
                    'class': 'logging.handlers.SysLogHandler',
                    'level': 'syslog_level' in log_cfg and log_cfg['syslog_level'] or 'INFO',
                    'formatter': 'syslog',
                    'facility': 'syslog_facility' in log_cfg and log_cfg[
                        'syslog_facility'] or 'user',
                    'address': syslog_address,
                    'socktype': syslog_socket
                },
                'file': {
                    'class': 'logging.FileHandler',
                    'level': 'log_level' in log_cfg and log_cfg['log_level'] or 'INFO',
                    'formatter': 'simple',
                    'filename': log_filename,
                    'encoding': 'utf-8'
                },
                'size_rotation_handler': {
                    'class': 'logging.handlers.RotatingFileHandler',
                    'level': 'log_level' in log_cfg and log_cfg['log_level'] or 'INFO',
                    'formatter': 'simple',
                    'filename': log_filename,
                    'maxBytes': int('file_size' in log_cfg and log_cfg['file_size'] or 0),
                    'backupCount': int('file_cnt' in log_cfg and log_cfg['file_cnt'] or 0)
                },
                'time_rotation_handler': {
                    'class': 'logging.handlers.TimedRotatingFileHandler',
                    'level': 'log_level' in log_cfg and log_cfg['log_level'] or 'INFO',
                    'formatter': 'simple',
                    'filename': log_filename,
                    'interval': int('interval' in log_cfg and log_cfg['interval'] or 0),
                    'when': 'interval' in log_cfg and log_cfg['interval_unit'] or 'S',
                    'backupCount': int('file_cnt' in log_cfg and log_cfg['file_cnt'] or 0)
                }
            },
            'root': {
                'level': 'INFO',
                # 'handlers': ['syslog', 'file']
                # 'handlers': ['console', 'syslog', 'file', 'size_rotation_handler', 'time_rotation_handler']
            }
        }
        handlers = ['file']
        if 'console' in log_cfg and log_cfg['console'] == 'enabled':
            handlers.append('console')
        if 'time_rotation' in log_cfg and log_cfg['time_rotation'] == 'enabled':
            handlers.append('time_rotation_handler')
        if 'size_rotation' in log_cfg and log_cfg['size_rotation'] == 'enabled':
            handlers.append('size_rotation_handler')
        if 'file' in log_cfg and log_cfg['file'] == 'disabled':
            handlers.remove('file')
        if 'syslog' in log_cfg and log_cfg['syslog'] == 'enabled':
            handlers.append('syslog')
        if not handlers:
            raise Exception('No handlers enabled in the configuration file')

        log_dict['root']['handlers'] = handlers
        logging.config.dictConfig(log_dict)
        cm_logger = logging.getLogger(self.module_name)
        self.logger = cm_logger
        # cm_logger.info(json.dumps(log_dict, indent=2))
        return cm_logger

    def change_log_level(self, level='INFO'):
        """change the log level of the logger
        level should be one of 'DEBUG', 'INFO', 'WARN', 'ERROR'
        """
        if self.logger:
            self.logger.setLevel(level)
            return True
        else:
            return False


if __name__ == "__main__":
    sample_config = """[logging]
log_dir=/var/log/nkv-agent/
log_file=cm_test_log.file
log_level = DEBUG
file = enabled
console_level=INFO
console=disabled
syslog=enabled
#syslog_address=('msl-dc-client4', 514)
#syslog_socket=udp
syslog_level = DEBUG
syslog_facility = local0
time_rotation=disabled
interval=1000
interval_unit=S
size_rotation=disabled
file_size=20000
file_cnt=5
"""
    conf_file = os.path.join('/tmp/test.conf')
    with open(conf_file, 'w') as f:
        f.write(sample_config)

    ll = CMLogger("Test", conf_file)
    x = ll.get_log_config_options()
    logger = ll.create_logger()
    for i in range(10):
        logger.info("Testing the code validation - %s" % i)
        time.sleep(2)
    ll.change_log_level('WARN')
    logger.info('INFO message should not show up')
    logger.warn('Shows warning message')
