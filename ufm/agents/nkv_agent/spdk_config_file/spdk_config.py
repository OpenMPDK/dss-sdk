import errno
import hashlib
import os
import re

from utils.log_setup import agent_logger


class SPDKConfig:
    def __init__(self, conf_path):
        self.conf_path = conf_path
        self.temp_conf_path = "%s~" % self.conf_path
        self.logger = agent_logger

    def read_subsystems(self, path):
        """
        Read Subsystem sections from the configuration file.
        :param path: Path where configuration file is located.
        :return: dict of subsystems
        """
        try:
            conf_fh = open(path, 'rb')
        except IOError as e:
            self.logger.exception("Error when attempting to read configuration file - %s" % str(e))
            raise
        lines = conf_fh.readlines()
        arr = []
        idx = -1
        section = None
        for line in lines:
            line = line.strip(str.encode('\r\n '))
            if line.startswith(str.encode("#")):
                continue
            line = line.split("#",1)[0]

            m = re.match(r"^(\[Subsystem.+?\])$", line)
            if m is not None:
                section = m.group(1)
                arr.append({section: {}})
                idx += 1
            else:
                if section is None:
                    continue
                try:
                    if line.startswith(str.encode("Listen")):
                        key = line.split()[1]
                        value = line.split()[2]
                        if key in arr[idx][section]:
                            value = arr[idx][section][key] + "," + value
                    elif line.startswith(str.encode("Namespace")):
                        key = line
                        value = ""
                    else:
                        key, value = line.split(str.encode(' '), 1)
                    arr[idx][section][key] = value
                except:
                    continue
        return arr

    def read_local_config(self, path):
        """
        Read configuration file and convert to in-memory array.
        :param path: Path where configuration file is located.
        :return:
        """
        try:
            conf_fh = open(path, 'rb')
        except IOError as e:
            self.logger.exception("Error when attempting to read configuration file - %s" % str(e))
            raise
        lines = conf_fh.readlines()
        arr = []
        idx = -1
        section = None
        for line in lines:
            if line.startswith(str.encode("#")):
                continue
            m = re.match(r"^(\[.+?\])$", line)
            if m is not None:
                section = m.group(1)
                arr.append({section: {}})
                idx += 1
            else:
                if section is None:
                    continue
                try:
                    if line.startswith(str.encode("TransportId")):
                        key, value = line.strip(str.encode('\r\n')).rsplit(str.encode(' '), 1)
                    elif line.startswith(str.encode("Listen")):
                        key = line.strip(str.encode('\r\n'))
                        value = ""
                    elif line.startswith(str.encode("Namespace")):
                        m = re.match(r"^(Namespace\s+\".+?\")\s+(\d+)$", line)
                        key = m.group(1)
                        value = m.group(2)
                    elif line.startswith(str.encode("Qos_host")):
                        m = re.match(r"^(Qos_host\s+.+?)\s+(\".+?\")$", line)
                        key = m.group(1)
                        value = m.group(2)
                    else:
                        key, value = line.strip(str.encode('\r\n')).split(str.encode(' '), 1)
                    arr[idx][section][key] = value
                except:
                    continue
        return arr

    def write_local_config(self, arr, path):
        """
        Write arr, which contains configuration file in
        text file format, to disk as a configuration file.
        :param arr: In-memory lines which represent a configuration file.
        :param path: Path where to write the file to.
        :return:
        """
        tmp_fh = open(path, 'wb')
        tmp_fh.write(str.encode("# AUTO-GENERATED FILE - DO NOT EDIT\n"))
        for section_key in arr:
            section = section_key.keys()[0]
            tmp_fh.write("%s\n" % section)
            for key in sorted(section_key[section]):
                value = section_key[section][key]
                tmp_fh.write(str.encode("%s %s\n" % (key, value)))
            tmp_fh.write(str.encode("\n"))
        tmp_fh.close()
        self.logger.info('local_config %s generated', path)

    @staticmethod
    def delete_subsystem_local_config(arr, subsystem_name):
        """
        Find a subsystem and remove it from arr.
        :param arr: In-memory lines which represent a configuration file.
        :param subsystem_name: Name to filter and remove.
        :return:
        """
        for idx, x in enumerate(arr):
            section = x.keys()[0]
            if section == subsystem_name:
                arr.pop(idx)
                break

    @staticmethod
    def update_namespaces_local_config(ns_arr, config):
        """
        Update the namespaces in the configuration file
        with the new config parameter.
        :param ns_arr: In-memory lines which represent a configuration file.
        :param config: New configuration with details to update in the
                       variable arr.
        :return:
        """
        nvme_section = None
        for _dict in ns_arr:
            section = _dict.keys()[0]
            if section.startswith("[Nvme]"):
                nvme_section = _dict
                break
        if nvme_section is None:
            nvme_section = {"[Nvme]": {}}
            ns_arr.append(nvme_section)

        for ns in config["namespaces"].values():
            sn = ns["Serial"]
            pci_addr = ns["PCIAddress"]
            new_transport_key = "TransportId \"trtype:PCIe traddr:%s\"" % pci_addr
            found = 0
            for transport_key in nvme_section["[Nvme]"]:
                if transport_key == new_transport_key:
                    found = 1
                    break
            if found == 0:
                nvme_section["[Nvme]"][new_transport_key] = "\"%s\"" % sn

    @staticmethod
    def append_subsystem_local_config(arr, config):
        """
        Append new subsystem entry to arr which will be
        written to the temporary config file.
        :param arr: Stores temporary config file in-memory.
        :param config: Dict containing all configuration information.
        :return:
        """
        subsystem_name = "[%s%s]" % ("Subsystem_", config["NQN"])
        serial_number = hashlib.sha1(config["NQN"].encode("UTF-8")).hexdigest()[:20]
        subsystem_dict = {subsystem_name: {"NQN": config["NQN"],
                                           "SN": serial_number,
                                           "AllowAnyHost": "Yes", }, }

        for address in config["transport_addresses"].values():
            listen_string = "Listen %s %s:%s" % (address["trtype"],
                                                 address["traddr"],
                                                 address["trsvcid"])
            subsystem_dict[subsystem_name][listen_string] = ""

        for ns in config["namespaces"].values():
            sn = ns["Serial"]
            nsid = ns["nsid"]
            subsystem_dict[subsystem_name]["Namespace \"%sn1\"" % sn] = nsid
        arr.append(subsystem_dict)

    def get_namespaces_local_config(self):
        """
        Parse the namespaces from configuration file and
        build an array of namespaces.
        :return:
        """
        arr = self.read_local_config(self.conf_path)
        namespaces_array = []
        for element in arr:
            section = element.keys()[0]
            if section == "[Nvme]":
                for ns in element.values()[0]:
                    key = ns
                    m = re.match(r"^TransportId \"trtype:PCIe traddr:([0-9A-Fa-f]{4}(?::[0-9A-Fa-f]{2}){2}\.\d)\"$",
                                 key)
                    if m is not None:
                        pci_addr = m.group(1)
                        serial = element[section][key]
                        namespaces_array.append({"PCIAddress": pci_addr,
                                                 "Serial": serial, })
        return namespaces_array

    def save_device_transport_to_config(self, devices):
        namespaces = self.get_namespaces_local_config()
        dev_to_config = {}
        for serial in devices:
            if devices[serial] not in namespaces:
                dev_to_config[serial] = devices[serial]
        arr = self.read_local_config(self.conf_path)
        if dev_to_config:
            self.update_namespaces_local_config(arr,
                                                {'namespaces': dev_to_config})
            namespaces = self.get_namespaces_local_config()
            self.write_local_config(arr, self.temp_conf_path)
            self.rename_temp_local_config()

    def save_temp_local_config(self, subsystem, delete):
        """
        Create temporary configuration file to save locally.
        Temporary file will be renamed to existing configuration
        file when command is carried out successfully.
        :param subsystem: Subsystem to update in temporary config file.
        :param delete: Deletion flag.
        :return:
        """
        arr = self.read_local_config(self.conf_path)
        self.update_namespaces_local_config(arr, subsystem)
        if delete:
            subsystem_conf_name = "[Subsystem_%s]" % subsystem["NQN"]
            self.delete_subsystem_local_config(arr, subsystem_conf_name)
        else:
            self.append_subsystem_local_config(arr, subsystem)
        self.write_local_config(arr, self.temp_conf_path)

    def add_subsystem_temp_local_config(self, subsystem):
        """
        Create a temporary configuration file and add a new
        subsystem to the temporary configuration file.
        :param subsystem: Subsystem to update in temporary config file.
        :return:
        """
        self.save_temp_local_config(subsystem, 0)

    def delete_subsystem_temp_local_config(self, subsystem):
        """
        Create a temporary configuration file and remove a
        subsystem from the temporary configuration file.
        :param subsystem: Subsystem to update in temporary config file.
        :return:
        """
        self.save_temp_local_config(subsystem, 1)

    def rename_temp_local_config(self):
        """
        Rename temporary configuration file to
        default configuration file.
        Used to handle atomic rename/replace.
        :return:
        """
        src = self.temp_conf_path
        dst = self.conf_path
        try:
            os.rename(src, dst)
        except Exception as e:
            print(e)
            raise

    def delete_temp_local_config(self):
        """
        Remove the temporary local configuration file.
        :return:
        """
        tmp = self.temp_conf_path
        try:
            os.remove(tmp)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise
