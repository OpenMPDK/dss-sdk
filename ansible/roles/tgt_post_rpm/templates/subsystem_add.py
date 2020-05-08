from subprocess import Popen, PIPE
from socket import gethostname
import sys
import uuid
import ConfigParser
import logging

kv_cli_path = "{{ kv_cli_location }}"
virtual_env = "{{ nkv_virtualenv_dir }}/bin/python"
config_file_location = "{{ config_file_dir }}/DFLY_CONFIG"

logging.basicConfig(format='%(asctime)s %(message)s', filename="{{ logfile_dir }}/subsystem_add.log", level=logging.DEBUG)


def ConfigSectionMap(conf_parse, section):
    """
    Parse config file sections.
    @param conf_parse: ConfigParser object
    @param section: Section to be parsed.
    @return: dictionary of values
    """
    dict1 = {}
    options = conf_parse.options(section)
    for option in options:
        try:
            dict1[option] = conf_parse.get(section, option)
            if dict1[option] == -1:
                logging.debug("skip: %s" % option)
        except:
            logging.error("exception on %s!" % option)
            dict1[option] = None
    return dict1


def get_machine_id():
    with open('/etc/machine-id') as f:
        machine_id = f.read().rstrip()
    server_uuid = str(uuid.uuid3(uuid.NAMESPACE_DNS, machine_id))
    return server_uuid


def exec_cmd(cmd):
    """
    Execute any given command on shell
    @param cmd: command to run
    @return: Return code, output, error if any.
    """
    p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
    out, err = p.communicate()
    out = out.decode()
    out = out.strip()
    err = err.decode()
    ret = p.returncode

    return ret, out, err


def get_drives_list():
    """
    Execute kv-cli list command and get the drives list from etcd db.
    @param: None
    @return: comma separated drives list.
    """
    server_id = get_machine_id()
    cmd = (virtual_env + " " + kv_cli_path +
           " kv list 127.0.0.1 23790 --server=" + server_id +
           " | grep -a 'Free Disks' | awk '{print $3}'")
    logging.debug("Command to list drives in system: %s", cmd)

    ret, drives_list, err = exec_cmd(cmd)
    return drives_list


def get_subsystem_cmds(target_info, target_data_drives, target_data_drive_list,
                       target_meta_drives, target_meta_drive_list,
                       subsystem_trtype):
    """
    Return formatted command string with all details, for creating subsystems.
    @param1 target_info: Target section info
    @param2 target_data_drives: Number of drives for target data pool
    @param3 target_data_drive_list: List of drives for target data pool
    @param4 target_meta_drives: Number of drives for target metadata pool
    @param5 target_meta_drive_list: List of drives for target metadata pool
    @param6 subsystem_trtype: Subsystem transport type ("TCP", "RDMA")
    @return Formatted data pool and metadata pool commands.
    """
    subsystem_ip_port = target_info['subsystem_ip_port']
    server_uuid = get_machine_id()
    cmd = virtual_env + " " + kv_cli_path + " kv add 127.0.0.1 23790 --server=" + server_uuid + " --ip=" + subsystem_ip_port
    cmd += " --trtype=" + subsystem_trtype

    nvme_list = get_drives_list()
    drive_list = nvme_list.split(',')
    logging.debug("Drive available in system: %s", drive_list)
    if len(drive_list) == 0:
        logging.error('No drives found to be used for subsystem creation. '
                      'Exiting')
        sys.exit(-1)

    data_drive_list, meta_drive_list = [], []
    if target_data_drive_list:
        target_data_drive_list = target_data_drive_list.replace(' ', '')
        data_drive_list = target_data_drive_list.split(',')
        for i, item in enumerate(data_drive_list):
            if item not in drive_list:
                logging.error('Drive %s in data drives not found', item)
                sys.exit(-1)
            if 'dev' not in item:
                data_drive_list[i] = item.replace('nvme', '/dev/nvme')

        logging.debug("DFLY_CONFIG: Data drive list: %s", data_drive_list)

    if target_meta_drive_list:
        target_meta_drive_list = target_meta_drive_list.replace(' ', '')
        meta_drive_list = target_meta_drive_list.split(',')
        for i, item in enumerate(meta_drive_list):
            if item not in drive_list:
                logging.error('Drive %s in metadrives not found', item)
                sys.exit(-1)
            if 'dev' not in item:
                meta_drive_list[i] = item.replace('nvme', '/dev/nvme')

        logging.debug("DFLY_CONFIG: Metadata drive list: %s", meta_drive_list)

    if len(drive_list) < int(target_data_drives) + int(target_meta_drives) or \
                    len(drive_list) < int(len(data_drive_list)) + int(
                len(meta_drive_list)):
        logging.error("Number of drives appear to be less than mentioned in "
                      "config file. Exiting...")
        sys.exit(0)

    data_pool_cmd, metadata_pool_cmd = None, None
    if int(target_data_drives) > 0 or target_data_drive_list:
        if target_data_drive_list:
            drives_for_data = ','.join(data_drive_list)
        else:
            drives_for_data = ",".join(
                (drive_list[x] for x in range(0, int(target_data_drives))))
        data_nqn = target_info['data_nqn']
        data_pool_cmd = cmd + " --devices=" + drives_for_data + " --nqn=" + data_nqn
    if int(target_meta_drives) > 0 or target_meta_drive_list:
        if target_meta_drive_list:
            drives_for_meta = ','.join(meta_drive_list)
        else:
            drives_for_meta = ",".join((drive_list[x] for x in
                                        range(int(target_data_drives),
                                              int(target_data_drives) + int(
                                                  target_meta_drives))))
        metadata_nqn = target_info['metadata_nqn']
        metadata_pool_cmd = cmd + " --devices=" + drives_for_meta + " --nqn=" + metadata_nqn

    return data_pool_cmd, metadata_pool_cmd


def create_subsystem(conf_parse, gb_dict, target_section):
    dp_drives, dp_drive_list, mt_drives, mt_drive_list = None, None, None, None

    logging.info('Creating subsystem for the target section %s',
                 target_section)
    if 'datapool_drives' in gb_dict:
        dp_drives = gb_dict['datapool_drives']
        logging.debug("Data drives: %s", dp_drives)
    if 'metapool_drives' in gb_dict:
        mt_drives = gb_dict['metapool_drives']
        logging.debug("Metadata drives: %s", mt_drives)
    if 'datapool_drive_list' in gb_dict:
        dp_drive_list = gb_dict['datapool_drive_list']
        logging.debug("Data drives_list: %s", dp_drive_list)
    if 'metapool_drive_list' in gb_dict:
        mt_drive_list = gb_dict['metapool_drive_list']
        logging.debug("Metadata drive_list: %s", mt_drive_list)

    logging.info("-----------My Target Section Details---------------")
    if target_section:
        target_info = ConfigSectionMap(conf_parse, target_section)
    else:
        logging.debug("DFLY_CONFIG: TARGET section %s is wrong or Not found. "
                      "Exiting...", target_section)
        return

    if 'datapool_drives' in target_info:
        target_data_drives = target_info['datapool_drives']
    elif dp_drives:
        target_data_drives = dp_drives
    else:
        target_data_drives = 0

    if 'metapool_drives' in target_info:
        target_meta_drives = target_info['metapool_drives']
    elif mt_drives:
        target_meta_drives = mt_drives
    else:
        target_meta_drives = 0

    logging.debug("Target Data drives: %s", target_data_drives)
    logging.debug("Target Metadata drives: %s", target_meta_drives)

    if 'datapool_drive_list' in target_info:
        target_data_drive_list = target_info['datapool_drive_list']
    elif dp_drive_list:
        target_data_drive_list = dp_drive_list
    else:
        target_data_drive_list = []

    if 'metapool_drive_list' in target_info:
        target_meta_drive_list = target_info['metapool_drive_list']
    elif mt_drive_list:
        target_meta_drive_list = mt_drive_list
    else:
        target_meta_drive_list = []

    if 'subsystem_trtype' in target_info:
        subsystem_trtype = target_info['subsystem_trtype']
    else:
        subsystem_trtype = 'TCP'

    logging.debug("Target Data drives list: %s", target_data_drive_list)
    logging.debug("Target Metadata drives list: %s", target_meta_drive_list)

    data_pool_cmd, metadata_pool_cmd = get_subsystem_cmds(target_info,
                                                          target_data_drives,
                                                          target_data_drive_list,
                                                          target_meta_drives,
                                                          target_meta_drive_list,
                                                          subsystem_trtype)

    if data_pool_cmd:
        logging.info(data_pool_cmd)
        ret, out, err = exec_cmd(data_pool_cmd)
        if ret != 0:
            if err:
                logging.error("Error in adding subsystem: %s", err)
            if out:
                logging.info("Output: %s", out)
        else:
            if out:
                logging.info("Output: %s", out)

    if metadata_pool_cmd:
        logging.info(metadata_pool_cmd)
        ret1, out1, err1 = exec_cmd(metadata_pool_cmd)
        if ret1 != 0:
            if err1:
                logging.error("Error in adding subsystem: %s", err1)
            if out1:
                logging.info("Output: %s", out1)
        else:
            if out1:
                logging.info("Output: %s", out1)

    logging.info("===============================================")


if __name__ == '__main__':

    hostname = gethostname().split('.', 1)[0]

    conf_parse = ConfigParser.ConfigParser()
    conf_parse.read(config_file_location)
    sections = conf_parse.sections()
    global_section, target_section = [], []
    for section in sections:
        if section == 'GLOBAL':
            global_section = section
            break
    logging.info("Global section name: %s", global_section)

    logging.info("-------GLOBAL section------")
    if global_section:
        gb_dict = ConfigSectionMap(conf_parse, global_section)
    else:
        logging.debug("DFLY_CONFIG: GLOBAL section is wrong or Not found.")
        sys.exit(-1)

    section_identifier = hostname + ':'
    for section in sections:
        if section_identifier in section or hostname == section:
            target_section = section
            logging.debug("Target section name: %s", target_section)
            create_subsystem(conf_parse, gb_dict, target_section)


