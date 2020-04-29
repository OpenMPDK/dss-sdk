import os,sys
import subprocess
import traceback
#import ntplib
import time


"""
Contains list of utility functions...
"""

def exception(func):
    """
    Implementation of nested function for decorator.
    :param func: <function ptr>
    :return: depends on function return type.
    """
    def wrapper(self, *args,**kwargs):
        try:
            return func(self,*args,**kwargs)
        except Exception as e:
            print("EXCEPTION: {} : {}".format(e,traceback.format_exc()))
            return False
    return wrapper


def exec_cmd(cmd="", output=False, blocking=False):
    """
    Execute the specified command
    :param cmd: <string> a executable command.
    :return: None
    """
    ret = 0
    console_output= ""
    std_out_default = sys.stdout
    try:
        print("INFO: Execution Cmd - {}".format(cmd))
        if blocking:
            if output:
                result = subprocess.check_output(cmd.split(), shell=False, stderr=subprocess.STDOUT,universal_newlines=False)
                console_output = result
            else:
                DEVNULL = open(os.devnull, "wb")
                ret = subprocess.call(cmd.split(), shell=False, stdout=DEVNULL, stderr=subprocess.STDOUT)
        else:
            subprocess.Popen(cmd.split())

    except subprocess.CalledProcessError as e:
        print(traceback.format_exc())
        console_output = e.output
        ret = e.returncode

    finally:
        if not output:
            os.stdout = std_out_default

    return ret , console_output

@exception
def ntp_time(host):
    """
    return the ntp server time for unified timing
    :return: <int> ntp time
    """
    """
    client = ntplib.NTPClient()
    response = client.request(host)
    ntp_time = int(response.tx_time)
    return ntp_time
    """
    pass

@exception
def epoch(ts):
    """
    Get epoche from timestamp
    :param ts:
    :return:
    """
    time_format = '%Y-%m-%d %H:%M:%S'
    ts_epoch =int( time.mktime(time.strptime(ts, time_format)))
    return ts_epoch

@exception
def get_file_path(base_dir, file_name):
    """
    Return absolute file path.
    :param base_dir: <string> a file base directory path
    :param file_name: <string> file name
    :return: <string> Complete file path.
    """
    file_path = os.path.abspath(base_dir + "/" + file_name)

    return file_path




