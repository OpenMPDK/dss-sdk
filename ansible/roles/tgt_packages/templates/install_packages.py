import os
import argparse
from subprocess import Popen, PIPE
# from distutils.version import StrictVersion

package_path = "{{ dfly_temp_dir }}/NKV-Release"


def exec_cmd(cmd):
    '''
    Execute any given command on shell
    @return: Return code, output, error if any.
    '''
    p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
    out, err = p.communicate()
    out = out.decode('utf-8')
    out = out.strip()
    err = err.decode('utf-8')
    ret = p.returncode

    return ret, out, err


def install_rpm(f_name):
    '''
    Install the given RPM file.
    '''
    cmd = "rpm -ivh " + f_name
    ret, out, err = exec_cmd(cmd)

    return ret, out, err


def get_version(f_name, package=False):
    '''
    Get version of packaged rpm file or installed rpm.
    '''
    if package:
        cmd = "rpm -qip"
    else:
        cmd = "rpm -qi"
    cmd = cmd + " " + f_name + " | grep -a 'Name\|Version' | awk '{print $3}' | paste -sd ':'"

    ret, name_version, err = exec_cmd(cmd)
    return name_version


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("package_names", type=str, help="display a list of given names")
    args = parser.parse_args()
    host_package_names = map(str, args.package_names.strip('[]').split(','))

    release_file_names = []
    for name in os.listdir(package_path):
        release_file_names.append(os.path.join(package_path, name))

    release_versions = {}
    for f in release_file_names:
        out = get_version(f, True)
        name, version = out.split(':')
        # release_versions[name] = StrictVersion(version)
        release_versions[name] = str(version)

    package_name_file = {}
    for name in os.listdir(package_path):
        for item in release_versions.keys():
            if name.startswith(item):
                package_name_file[item] = os.path.join(package_path, name)

    for name in host_package_names:
        install_rpm(package_name_file[name])
