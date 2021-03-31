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

import configobj
import os

diamond_default_file = '/etc/diamond/diamond.conf'


class DiamondConf(object):
    def __init__(self, config_file=None):
        self.config_file = config_file or diamond_default_file

    def get_graphite_host(self):
        ipaddr = None
        port = None
        co = configobj.ConfigObj(self.config_file)
        try:
            ipaddr = co['handlers']['GraphiteHandler']['host']
            port = co['handlers']['GraphiteHandler']['port']
        except Exception as e:
            print('Exception in updating graphite host', str(e))
            raise e
        return ipaddr, port

    def update_graphite_host(self, ipaddr=None, port=None):
        if not all([ipaddr, port]):
            return
        co = configobj.ConfigObj(self.config_file, list_values=False)
        try:
            if ipaddr:
                co['handlers']['GraphitePickleHandler']['host'] = ipaddr
            if port:
                co['handlers']['GraphitePickleHandler']['port'] = port
        except Exception as e:
            print('Exception in updating graphite host', str(e))
            raise e
        co.write()

    def get_statsd_host(self):
        ipaddr = None
        port = None
        co = configobj.ConfigObj(self.config_file)
        try:
            ipaddr = co['handlers']['StatsdHandler']['host']
            port = co['handlers']['StatsdHandler']['port']
        except Exception as e:
            print('Exception in updating statsd host', str(e))
        return ipaddr, port

    def update_statsd_host(self, ipaddr=None, port=None):
        if not all([ipaddr, port]):
            return
        co = configobj.ConfigObj(self.config_file, list_values=False)
        try:
            if ipaddr:
                co['handlers']['StatsdHandler']['host'] = ipaddr
            if port:
                co['handlers']['StatsdHandler']['port'] = port
        except Exception as e:
            print('Exception in updating statsd host', str(e))
            raise e
        co.write()

    def get_metrics_prefix(self):
        prefix_str = None
        host_name = None
        co = configobj.ConfigObj(self.config_file)
        try:
            prefix_str = co['collectors']['default']['path_prefix']
            host_name = co['collectors']['default']['hostname']
        except Exception as e:
            print('Exception in updating graphite host', str(e))
        return prefix_str, host_name

    def update_metrics_prefix(self, prefix_str=None, host_name=None):
        if not all([prefix_str, host_name]):
            return
        co = configobj.ConfigObj(self.config_file, list_values=False)
        try:
            if prefix_str:
                co['collectors']['default']['path_prefix'] = prefix_str
            if host_name:
                co['collectors']['default']['hostname'] = host_name
        except Exception as e:
            print('Exception in updating graphite host', str(e))
            raise e
        co.write()

    @staticmethod
    def restart_service():
        ret = os.system('systemctl restart diamond.service')
        return ret

    def update_diamond_conf(self, proto, ipaddr=None, port=None, prefix_str=None, host_name=None):
        if not os.access(self.config_file, os.R_OK):
            return
        update = False
        co = configobj.ConfigObj(self.config_file, list_values=False)
        if proto == 'graphite':
            if ipaddr and co['handlers']['GraphitePickleHandler']['host'] != ipaddr:
                co['handlers']['GraphitePickleHandler']['host'] = ipaddr
                update = True
            if port and co['handlers']['GraphitePickleHandler']['port'] != port:
                co['handlers']['GraphitePickleHandler']['port'] = port
                update = True
        elif proto == 'statsd':
            if ipaddr and co['handlers']['StatsdHandler']['host'] != ipaddr:
                co['handlers']['StatsdHandler']['host'] = ipaddr
                update = True
            if port and co['handlers']['StatsdHandler']['port'] != port:
                co['handlers']['StatsdHandler']['port'] = port
                update = True

        if prefix_str and co['collectors']['default']['path_prefix'] != prefix_str:
            co['collectors']['default']['path_prefix'] = prefix_str
        if host_name and co['collectors']['default']['hostname'] != host_name:
            co['collectors']['default']['hostname'] = host_name

        if update:
            co.write()
            self.restart_service()



