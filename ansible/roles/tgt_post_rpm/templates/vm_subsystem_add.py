from subprocess import Popen, PIPE
import uuid
import argparse
import logging

kv_cli_path = "/usr/share/nkvagent/kv-cli.py"
default_config_file = "/etc/dragonfly/nvmf.in.conf"

logging.basicConfig(format='%(asctime)s %(message)s',
                    filename="/var/log/dragonfly/vm_subsystem_add.log",
                    level=logging.DEBUG)


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


if __name__ == '__main__':

    logging.info("==============Start vm_subsystem_add============")

    parser = argparse.ArgumentParser(description=
             "This program parses input nvmf.in.conf file, and adds subsystems to etcd3.",
             formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("-c", "--config_file", nargs='?', type=str, default=default_config_file, 
                        help="Configuration file.")
    parser.add_argument("-t", "--trtype", nargs='?', type=str, default="rdma", 
                        help="Transportation type, e.g. tcp, rdma.")

    args = parser.parse_args()

    server_id = get_machine_id()
    cmd = (kv_cli_path +
           " kv add_from_file 127.0.0.1 23790 --server=" + server_id +
           " --config_file=" + args.config_file + " --trtype=" + args.trtype)
    logging.info("Command to add subsystems in vm: %s", cmd)

    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        if err:
            logging.error("Error in adding subsystem: %s", err)
        if out:
            logging.info("Output: %s", out)
    else:
        if out:
            logging.info("Output: %s", out)

    logging.info("==============End vm_subsystem_add===============")

