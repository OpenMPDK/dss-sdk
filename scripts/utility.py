import os,sys
import subprocess
import traceback


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
            print("EXCEPTION-ddd: {} : {}".format(e,traceback.format_exc()))
    return wrapper

def exec_cmd(cmd=""):
    """
    Execute the specified command
    :param cmd: <string> a executable command.
    :return: None
    """
    ret = 0
    console_output= ""
    try:
        print("INFO: Execution Cmd - {}".format(cmd))
        result = subprocess.check_output(cmd.split(), shell=False, stderr=subprocess.STDOUT,universal_newlines=False)
        console_output = result
    except subprocess.CalledProcessError as e:
        print(traceback.format_exc())
        console_output = e.output
        ret = e.returncode

    return ret , console_output





