import time
import threading
import zlib
import json

from subprocess import PIPE, Popen


class Node_Health(threading.Thread):
    def __init__(self, stopper_event=None, db=None, hostname=None, check_interval=60, log=None):
        super(Node_Health, self).__init__()
        self.stopper_event = stopper_event
        self.db = db
        self.hostname = hostname
        self.check_interval = check_interval
        self.log = log
        self.log.info("Init {}".format(self.__class__.__name__))

        self.tgt_node_key = '/object_storage/servers/{uuid}/node_status'
        self.tgt_interface_key = '/object_storage/servers/{uuid}/server_attributes' \
                                 '/network/interfaces/{if_name}/Status'

        self.tgt_interface_crc = '/object_storage/servers/{uuid}/server_attributes' \
                                 '/network/interfaces/{if_name}/CRC'

    def __del__(self):
        pass

    def _check_interfaces(self, ifaces, kv_dict, server):
        """
        The previous update of heartbeat for the server is more than
        twice the HB interval. Let's ping the IP address and see if
        there is any issue with the node
        """
        ipaddr_dict = {}

        for iface in ifaces:
            if 'IPv4' in ifaces[iface]:
                ipaddr_dict[ifaces[iface]['IPv4']] = iface

        if not len(ipaddr_dict):
            return False

        # Run fping command on all the IP addresses at once. The
        # output will show up on stderr only as following
        # 10.1.30.134    : xmt/rcv/%loss = 3/0/100%
        # 111.100.10.121 : xmt/rcv/%loss = 3/3/0%
        cmd = 'fping -q -c 3 ' + ' '.join(list(ipaddr_dict.keys()))
        pipe = Popen(cmd.split(), stdout=PIPE, stderr=PIPE)
        out, err = pipe.communicate()

        self.log.debug('fping cmd %s out [%s] err [%s]', cmd, out, err)

        if err:
            lines_list = err.decode("utf-8").split('\n')
            for line in lines_list:
                if not len(line):
                    continue

                ipaddr_out = line.split(':')[0].strip()
                iface = ipaddr_dict[ipaddr_out]

                self.log.detail("Updting NIC status for iface: [%s]", iface)

                # Update the NIC status and the CRC on the DB
                if 'min/avg/max' not in line:
                    self.log.detail("min/avg/max not detected in line: [%s]", line)
                    if ifaces[iface]['Status'] != 'down':
                        self.log.detail("iface status not previously down, changing to down")
                        self.log.debug('interface: [%s], down', iface)

                        ifaces[iface]['Status'] = 'down'
                        crc = zlib.crc32(json.dumps(ifaces[iface], sort_keys=True).encode())
                        ifaces[iface]['CRC'] = crc
                        kv_dict[self.tgt_interface_key.format(uuid=server, if_name=iface)] = 'down'
                        kv_dict[self.tgt_interface_crc.format(uuid=server, if_name=iface)] = str(crc)
                    else:
                        self.log.detail("NIC previously down, skipping")
                elif ifaces[iface]['Status'] != 'up':
                    self.log.detail("iface status not previously up, changing to up")
                    ifaces[iface]['Status'] = 'up'
                    crc = zlib.crc32(json.dumps(ifaces[iface], sort_keys=True).encode())
                    ifaces[iface]['CRC'] = crc
                    kv_dict[self.tgt_interface_key.format(uuid=server, if_name=ipaddr_dict[ipaddr_out])] = 'up'
                    kv_dict[self.tgt_interface_crc.format(uuid=server, if_name=iface)] = str(crc)
                    return True
                else:
                    self.log.detail("NIC not detected, and not previously up, skipping")

        return False

    def run(self):
        """
        This function checks the heartbeat value of each target node. If it was
        last updated more than twice the heartbeat interval, then try to ping the
        system using all the IPs available. If one of the IP is pingable,
        then mark the node as UP, but the respective down NICs will be marked as
        'down' along with the respective CRC field for the interface. If the CRC
        field is not updated, then the agent will not update the new value on
        detection.
        Also if the machine is pingable and the heartbeat value is alive, then we
        assume that the agent will update status of the NICs.
        """
        self.log.info("Start {}".format(self.__class__.__name__))

        global g_servers_out

        g_servers_out = None
        try:
            tgt_node_status_lease = self.db.lease(4 * self.check_interval)
        except Exception as ex:
            self.log.error('Error in creating lease for target nodes status')
            self.log.error("Exception: {} {}".format(__file__, ex))
            tgt_node_status_lease = None

        new_ip_status = {}

        while not self.stopper_event.is_set():
            self.stopper_event.wait(self.check_interval)

            # Get all the target nodes and its attributes
            try:
                servers_out = self.db.get_with_prefix('/object_storage/servers')
            except Exception as ex:
                self.log.error("Failed to connect to db")
                self.log.error("Exception: {} {}".format(__file__, ex))
                continue

            if not servers_out:
                self.log.info("===> No server found")
                continue

            try:
                g_servers_out = servers_out['object_storage']['servers']

                hb_list = []
                if 'list' in g_servers_out:
                    hb_list = g_servers_out.pop('list')
                    # self.log.info("Server(s) found: {}".format(hb_list))
            except Exception as ex:
                self.log.warning("Failed to get list of servers")
                self.log.error("Exception: {} {}".format(__file__, ex))
                g_servers_out = None
                hb_list = []
                continue

            kv_dict = dict()
            old_ip_status = new_ip_status

            if g_servers_out:
                strings_with_lease = []
                for server in g_servers_out:
                    try:
                        hb_time = hb_list[server]
                    except Exception:
                        hb_time = 0

                    if ((int(time.time()) - int(hb_time)) < (4 * self.check_interval)):
                        continue

                    try:
                        server_name = g_servers_out[server]['server_attributes']['identity']['Hostname']
                    except Exception:
                        self.log.warning('Failed to get hostname of server from DB')
                        continue

                    try:
                        ifaces = g_servers_out[server]['server_attributes']['network']['interfaces']
                    except Exception:
                        self.log.error('Failed to get interfaces of server from DB')
                        continue

                    # ===> Heartbeat last update is too old on server <===
                    status = self._check_interfaces(ifaces, kv_dict, server)

                    # Update the node status. If one of the NICs pingable, then 'up'
                    if server_name not in old_ip_status:
                        old_ip_status[server_name] = status

                    new_ip_status[server_name] = status
                    if old_ip_status[server_name] != new_ip_status[server_name]:
                        key = self.tgt_node_key.format(uuid=server)
                        if status:
                            kv_dict[key] = 'up'
                        else:
                            kv_dict[key] = 'down'

                        strings_with_lease.append(key)

                        self.log.info("NIC on target {} has changed to {}".format(server, kv_dict[key]))
                try:
                    if kv_dict:
                        self.log.debug("KVs updated to ETCD DB {}".format(kv_dict))
                        tgt_node_status_lease.refresh()
                        self.db.put_multiple(kv_dict, strings_with_lease, tgt_node_status_lease)
                except Exception as ex:
                    self.log.exception('Failed to save target node status to DB')
                    self.log.error("Exception: {} {}".format(__file__, ex))

    def stop(self):
        self.log.info("Node health check thread stopped")
        pass
