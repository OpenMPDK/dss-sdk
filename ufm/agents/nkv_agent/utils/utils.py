import os
import psutil
import re
import sched
import socket
import time
import ast
from log_setup import get_logger

log = get_logger()


def valid_ip(address):
    try:
        socket.inet_aton(address)
        return True
    except:
        return False


def flat_dict_generator(indict, pre=None):
    """
    Recursive dictionary object to generator. Allows
    flat traversal of all dictionary key-values.
    :param indict: Recursive lower level of dict.
    :param pre: Parent keys to concatenate with child keys.
    :return: 
    """
    pre = pre[:] if pre else []
    if isinstance(indict, dict):
        for key, value in indict.items():
            if isinstance(value, dict):
                for d in flat_dict_generator(value, pre + [key]):
                    yield d
            elif isinstance(value, list) or isinstance(value, tuple):
                for v in value:
                    for d in flat_dict_generator(v, pre + [key]):
                        yield d
            else:
                yield pre + [key, value]
    else:
        list_value = pre
        list_value.append(indict)
        yield list_value
        # yield indict


def read_linux_file(path):
    buf = None
    try:
        with open(path) as f:
            buf = str((f.read().rstrip()))
    finally:
        return buf


def time_delta(timestamp):
    now = time.time()
    elapsed = int(now - float(timestamp))
    return elapsed


class KVLog:
    SUCCESS = 1
    WARN = 2
    ERROR = 3

    @staticmethod
    def kvprint(status, msg):
        if status == KVLog.SUCCESS:
            color = "1;37;42"
        elif status == KVLog.WARN:
            color = "1;37;43"
        elif status == KVLog.ERROR:
            color = "1;37;41"
        else:
            color = "0"
        print(('\x1b[%sm' % color) +
              msg +
              '\x1b[0m')


def match_cmds_with_process(cmds, proc_args, lazy_regex):
    match = True

    if not cmds:
        return match

    for cmd in cmds:
        if cmd not in proc_args:
            m = None
            if lazy_regex:
                for arg in proc_args:
                    m = re.match(r"^.*?%s.*?$" % cmd, arg)
                    if m is not None:
                        break
            if m is None:
                match = False
                break
        else:
            proc_args.remove(cmd)
    return match


def pidfile_is_running(regex, cmds, pidfile):
    """
    Try to open pid file to read the pid.
    Check if process with read pid is running.
    See if the running process matches our regex and
    cmds we expect it to have.
    :param regex: Process name regex to match.
    :param cmds: Process arguments to compare and match.
    :param pidfile: Path to read/write pid file.
    :return: Running PID otherwise 0.
    """
    if os.path.isfile(pidfile):
        try:
            fh = open(pidfile, 'rb')
        except Exception as e:
            raise e
        try:
            pid_rd = int(fh.read())
        except:
            fh.close()
            return 0
        try:
            p = psutil.Process(pid_rd)
        except psutil.NoSuchProcess:
            return 0
        except Exception as e:
            raise e

        p_name_found = cmds_match = False
        m = re.match(regex, p.name())
        if m is not None:
            p_name_found = True
            cmds_match = match_cmds_with_process(cmds, p.cmdline(), 1)

        if p_name_found and cmds_match:
            return pid_rd
    return 0


def find_process_pid(pname_regex, cmds):
    """
    Detect if given process name is running with
    given cmds.
    :param pname_regex: Process name regex to match.
    :param cmds: Process cmd regex to match.
    :return: Running PID otherwise 0.
    """
    p_name_found = cmds_match = False
    # Ignore finding this process
    cur_pid = os.getpid()
    # Ignore finding the parent of this process
    par_pid = os.getppid()
    for proc in psutil.process_iter(attrs=['ppid', 'pid', 'name']):
        if cur_pid == proc.info["pid"] or \
           par_pid == proc.info["pid"]:
            continue

        m = re.match(pname_regex, proc.info["name"])
        if m is not None:
            p_name_found = True
            cmds_match = match_cmds_with_process(cmds, proc.cmdline(), 1)

        if p_name_found and cmds_match:
            return proc.info["pid"]
        else:
            p_name_found = cmds_match = False
    return 0


def check_spdk_running(nvmf_tgt_pidfile='nvmf_tgt.pid'):
    """
    Return if given process is running and its pid.
    :return: process_running (0/1), pid
    """
    process_running = 0
    proc_name = re.compile(r"^reactor_\d+$")
    pid = pidfile_is_running(proc_name, [], nvmf_tgt_pidfile)
    if pid > 0:
        process_running = 1
    else:
        cmds = ["nvmf_tgt", ]
        pid = find_process_pid(proc_name, cmds)
        if pid > 0:
            process_running = 1
    return pid, process_running


def _eval(string_value=None):
    """
    Convert string value to strings, bytes, numbers, tuples, lists, dicts, sets, booleans, and None
    :param string_value:
    :return:
    """
    try:
        string_value = ast.literal_eval(string_value)
    except Exception as e:
        log.exception(str(e))

    return string_value
