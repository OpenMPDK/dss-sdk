from distutils.version import StrictVersion
import logging
import os
import platform
from subprocess import Popen, PIPE
import sys
import time

package_path = "{{ dfly_temp_dir }}/NKV-Release"

etcd_config = '/etc/etcd/etcd.conf'
agent_config = '/etc/dragonfly/agent.conf'
etcd_updated = False
agent_updated = False
rest_updated = False
target_updated = False

log_file = '/var/log/dragonfly/upgrade.log'

agent_service = "kv-cli.service"
target_service = "nvmf_tgt.service"
etcd_service = "etcd.service"
rest_service = "gunicorn.service"
monitor_service = "etcdmonitor.service"
etcdgw_service = "etcd-gateway.service"

release_versions = {}
installed_versions = {}
package_name_file = {}
hostname = platform.node().split('.', 1)[0]

logging.basicConfig(
    filename=log_file, level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s - %(pathname)s [%(lineno)d] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S')


def exec_cmd(cmd):
    """
    Execute any given command on shell
    @return: Return code, output, error if any.
    """
    p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
    out, err = p.communicate()
    # out = out.decode('utf-8')
    out = out.strip()
    # err = err.decode('utf-8')
    ret = p.returncode

    logging.info("cmd:[%s],ret:[%s], out:[%s], err:[%s]", cmd, ret, out, err)
    return ret, out, err


def service_cmd(service_name, op):
    """
    General function to start/stop services
    """
    cmd = 'service ' + service_name + ' ' + op
    exec_cmd(cmd)


def get_version(f_name, package=False):
    """
    Get version of packaged rpm file or installed rpm.
    """
    if package:
        cmd = "rpm -qip"
    else:
        cmd = "rpm -qi"
    cmd = (cmd + " " + f_name + " | grep -a 'Name\|Version' | awk '{print "
                                "$3}' | paste -sd ':'")
    #    print cmd
    #    logging.debug("Command to list Version: %s", cmd)

    ret, name_version, err = exec_cmd(cmd)
    return name_version


def install_rpm(f_name):
    """
    Update the given RPM file.
    """
    cmd = "rpm -Uvh " + f_name
    ret, out, err = exec_cmd(cmd)
    return ret, out, err


def downgrade_rpm(f_name):
    """
    Downgrade the given RPM file.
    """
    cmd = "rpm -Uvh --force " + f_name
    ret, out, err = exec_cmd(cmd)
    return ret, out, err


def check_etcd_version():
    """
    Get etcd version
    """
    cmd = "ETCDCTL_API=3 etcdctl version | grep etcdctl | awk '{print $3}'"
    ret, version, err = exec_cmd(cmd)
    logging.info('etcd version installed %s', version)

    return version


def check_etcd_health():
    """
    Check etcd health before upgrading
    """
    cmd = "ETCDCTL_API=3 etcdctl member list | awk '{print $5}' | paste -sd,"
    ret, endpoints, err = exec_cmd(cmd)

    # print endpoints
    health_status = True
    health_cmd = "ETCDCTL_API=3 etcdctl endpoint health --endpoints=" + endpoints
    ret, health_status_output, err = exec_cmd(health_cmd)

    if 'unhealthy' in health_status_output:
        health_status = False

    return health_status


def update_etcd(f_name):
    """
    Upgrade etcd rpm
    """
    update_status = False
    sleep_cmd = "sleep 120"

    etcd_version_before = check_etcd_version()

    service_cmd(etcd_service, 'stop')
    install_rpm(f_name)
    exec_cmd(sleep_cmd)

    # Start service only if it's a FM node.
    if os.path.isfile(etcd_config):
        service_cmd(etcd_service, 'start')

    etcd_version_after = check_etcd_version()

    if StrictVersion(etcd_version_after) > StrictVersion(etcd_version_before):
        update_status = True

    return update_status


def rollback_packages(package_list):
    '''
    package_list.reverse()
    for pkg in package_list:
        logging.info('Rollback the package %s', pkg)

    cmd = ("ETCDCTL_API=3 etcdctl put /software/upgrade/progress/node/" +
           hostname + "/rollback Done")
    ret, _, _ = exec_cmd(cmd)
    if ret != 0:
        logging.error('Failed to run command %s', cmd)
    '''
    update_progress_in_db('node_rollback_flag', 'yes')
    return


def update_progress_in_db(module, status):
    cmd = ("ETCDCTL_API=3 etcdctl put /software/upgrade/progress/node/" +
           hostname + "/" + module + " " + status)
    ret, _, _ = exec_cmd(cmd)
    if ret != 0:
        logging.error('Failed to run command %s', cmd)


def update_rpm_version_in_db(package, version):
    cmd = ("ETCDCTL_API=3 etcdctl put /software/upgrade/progress/node/" +
           hostname + "/version/" + package + " " + str(version))
    ret, _, _ = exec_cmd(cmd)
    if ret != 0:
        logging.error('Failed to run command %s', cmd)


def check_services_after_upgrade():
    """
    Get the service status from ETCD DB, which got updated from the RPM
    itself, and check if the service status is 'up'
    :return:
    """
    status = False
    cmd = ("ETCDCTL_API=3 etcdctl get --prefix "
           "/software/upgrade/progress/node/" + hostname + '/services')
    ret, out, err = exec_cmd(cmd)
    if ret != 0:
        logging.error('Failed to run command %s', cmd)
        return status
    service_out = out.split('\n')
    if len(service_out) < 2:
        return True
    if len(service_out) % 2:
        logging.error('Something wrong with the output %s', str(service_out))
        return status
    for i in range(0, len(service_out), 2):
        if service_out[i+1] != 'up':
            logging.error('Service %s is %s', service_out[i], service_out[i+1])
            return status
    status = True
    return status


def check_and_upgrade_package():
    packages_upgraded = []
    if 'etcd' in installed_versions:
        update_progress_in_db('etcd', 'started')
        if release_versions['etcd'] > installed_versions['etcd']:
            healthy = check_etcd_health()
            if not healthy:
                update_progress_in_db('failure_reason',
                                      'cluster unhealthy in pre-upgrade')
                logging.error('One or more member(s) is not healthy. Aborting '
                            'upgrade')
                return False
            etcd_updated = update_etcd(package_name_file['etcd'])
            if not etcd_updated:
                update_progress_in_db('etcd', 'Failed')
                update_progress_in_db('failure_reason',
                                      'failed to update etcd RPM')
                logging.error("Failed to upgrade ETCD package")
                return False
            ver = get_version('etcd', False)
            if release_versions['etcd'] != ver:
                update_progress_in_db('etcd', 'Failed')
                update_progress_in_db('failure_reason',
                                      'failed to update etcd RPM')
                logging.error('etcd package verion not up to date')
                return False
            update_rpm_version_in_db('etcd', release_versions['etcd'])
            healthy = check_etcd_health()
            if not healthy:
                update_progress_in_db('failure_reason',
                                      'cluster unhealthy in post-upgrade')
                logging.error("One or more member(s) is not healthy after "
                              "upgrade.")
                return False
            packages_upgraded.append('etcd')
        else:
            logging.info('ETCD package is current')

        update_progress_in_db('etcd', 'current')

    packages = {'FM-Base': 'base', 'FM-Rest-Monitor': 'monitor',
                'NKV': 'nkv', 'FM-Agent': 'agent'}

    for key, value in packages.iteritems():
        if key in installed_versions:
            update_progress_in_db(value, 'started')
            if release_versions[key] > installed_versions[key]:
                ret, _, _ = install_rpm(package_name_file[key])
                if ret:
                    update_progress_in_db(value, 'Failed')
                    update_progress_in_db('failure_reason',
                                          'failed to update ' + key + ' RPM')
                    logging.error("Failed to upgrade " + key + " package")
                    rollback_packages(packages_upgraded)
                    return False
                ver = get_version(key, False)
                if release_versions[key] != ver.split(':')[1]:
                    update_progress_in_db(key, 'Failed')
                    update_progress_in_db('failure_reason',
                                          'failed to update ' + key + ' RPM')
                    logging.error(key + ' package verion not up to date')
                    rollback_packages(packages_upgraded)
                    return False
                update_rpm_version_in_db(value, release_versions[key])

                status = check_services_after_upgrade()
                if not status:
                    logging.error('Some services are not running')
                    rollback_packages(packages_upgraded)
                    return False
                packages_upgraded.append(key)

            update_progress_in_db(value, 'current')
        else:
            logging.info('Package %s is current', key)

    status = check_services_after_upgrade()
    if status:
        update_progress_in_db('node_service_status', 'up')
    else:
        update_progress_in_db('node_service_status', 'down')
    update_progress_in_db('node_package_version', 'current')
    return True


if __name__ == '__main__':

    update_progress_in_db('timestamp/start_epoch', str(int(time.time())))
    update_progress_in_db('node_status', 'in-progress')

    ret_code = 0
    release_file_names = []
    for name in os.listdir(package_path):
        release_file_names.append(os.path.join(package_path, name))

    # print release_file_names
    for f in release_file_names:
        ver = get_version(f, True)
        name, version = ver.split(':')
        # release_versions[name] = float(version)
        release_versions[name] = StrictVersion(version)

    logging.info("release_versions: %s", str(release_versions))

    for f in release_versions.keys():
        ver = get_version(f, False)
        if ver and len(ver):
            name, version = ver.split(':')
            # installed_versions[name] = float(version)
            installed_versions[name] = StrictVersion(version)
            if release_versions[name] < installed_versions[name]:
                logging.error('Version for %s [%s] is lower than the '
                              'installed version [%s]', name,
                              release_versions[name],
                              installed_versions[name])
                sys.exit(-1)

    logging.info("installed_versions: %s", str(installed_versions))

    for name in os.listdir(package_path):
        for item in release_versions.keys():
            if name.startswith(item):
                package_name_file[item] = os.path.join(package_path, name)

    ret_code = check_and_upgrade_package()
    if ret_code:
        update_progress_in_db('node_status', 'success')
    else:
        update_progress_in_db('node_status', 'failed')

    update_progress_in_db('timestamp/end_epoch', str(int(time.time())))

