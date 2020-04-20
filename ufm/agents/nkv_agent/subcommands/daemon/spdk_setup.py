import errno
import os
import subprocess
import sys

import constants
from utils.log_setup import agent_logger
from utils.utils import *

class SPDKSetup:
    def __init__(self, spdk_socket_path,
                 process_path, nvmf_conf_file,
                 minimum_nr_hugepage, nvmf_log_file,
                 spdk_conf, driver_setup):
        global logger
        logger = agent_logger

        self.spdk_socket_path = spdk_socket_path
        self.process_path = process_path
        self.nvmf_conf_file = nvmf_conf_file

        self.minimum_nr_hugepage = minimum_nr_hugepage
        self.nvmf_tgt_pidfile = "nvmf_tgt.pid"
        self.nvmf_tgt_stdout = nvmf_log_file
        self.nvmf_tgt_stderr = nvmf_log_file

        self.pid, self.process_running = check_spdk_running()
        self.spdk_conf = spdk_conf
        self.driver_setup = driver_setup

    def create_nvmf_conf(self):
        buf = constants.NVMF_CONF_HEADER
        if not os.path.exists(self.nvmf_conf_file):
            try:
                os.makedirs(os.path.dirname(self.nvmf_conf_file))
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise
            try:
                dummy_path = self.nvmf_conf_file + '.dummy'
                tmp_file_handle = open(dummy_path, 'wb')
                tmp_file_handle.write(str.encode(buf))
                tmp_file_handle.close()
                os.rename(dummy_path, self.nvmf_conf_file)
            except Exception as e:
                logger.exception('Exception in updating the NVMF conf file')
                raise e
        return

    def namespace_setup(self, devices):
        self.spdk_conf.save_device_transport_to_config(devices)
        namespaces = self.spdk_conf.get_namespaces_local_config()
        for ns in namespaces:
            try:
                pci_addr = ns["PCIAddress"]
                driver = self.driver_setup.get_driver_name(pci_addr)
                if driver != "uio_pci_generic":
                    self.driver_setup.setup("uio_pci_generic", pci_addr)
            except Exception as e:
                logger.exception('Exception in setting up the driver for %s',
                                 ns['PCIAddress'])
                raise e

    @staticmethod
    def generate_core_mask(core_count, dedicate_core_percent):
        mask_count = 64
        mask = 0xFFFFFFFFFFFFFFFF
        while core_count > mask_count:
            mask_count *= 2
            mask = (1 << mask_count) - 1
        core_half = int((core_count * dedicate_core_percent) / 2)
        # Temporary fix to limit cores for Dragonfly
        # Only use 32 total cores for Dragonfly
        if core_half > 16:
            core_half = 16
        core_allow = core_half * 2
        avail_mask = mask >> (mask_count - core_count)
        reserve_cores = core_count - core_allow
        reserve_mask = mask >> (mask_count - reserve_cores)
        core_mask = "{0:#x}".format((mask & ~(reserve_mask << core_half)) & avail_mask)
        return core_mask

    def launch_spdk_nvmf_tgt(self, core_count):
        if self.process_running:
            logger.info("nvmf_tgt(%d) already running" % self.pid)
            return self.pid

        subprocess.call(['modprobe', 'uio_pci_generic'])

        try:
            os.remove(self.spdk_socket_path)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise

        # Only use about 67% of the cores
        # TODO Change core usage for different systems
        # Mission Peak will use 32 out of the 48 cores
        core_mask = self.generate_core_mask(core_count, 0.667)
        obj = subprocess.Popen(['nohup', self.process_path, '-c', '%s' %
                                self.nvmf_conf_file,
                                '-r', '/var/run/spdk.sock',
                                '-m', core_mask],
                               stdout=open(self.nvmf_tgt_stdout, 'a'),
                               stderr=subprocess.STDOUT,
                               preexec_fn=os.setpgrp)
        if obj is not None:
            self.pid = obj.pid
            self.process_running = 1
            return obj.pid
        else:
            return None

    @staticmethod
    def get_hugetlbfs_mount():
        """
        Retrieve location of HugeTLB FS mount directory from
        'mount' command.
        :return: String path of HugeTLB FS mount directory, empty if not found.
        """
        hugetlbfs_mount = ""
        try:
            mount_output = read_linux_file('/proc/mounts').split('\n')
            for line in mount_output:
                if line.startswith('hugetlbfs'):
                    hugetlbfs_mount = line.split()[1]
                    break
        except ValueError:
            logger.info("Failed to retrieve hugetlbfs mount directory")
        except Exception as e:
            logger.info("get_hugetlbfs_mount failed - %s" % str(e))
            raise e
        return hugetlbfs_mount

    @staticmethod
    def mount_hugetlbfs(mount_dir):
        """
        Helper function to create hugepage mount directory
        :param mount_dir: Mount point for hugetlbfs FS
        :raise Exception in case of any exception
        """
        #
        try:
            os.makedirs(mount_dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                logger.info("Error in making hugetlbfs mount dir", exc_info=True)
                raise e

        args = ["mount", "-t", "hugetlbfs", "nodev", mount_dir]
        try:
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            output, error = process.communicate()
            if error:
                logger.info("Error:  " + error)
                raise ValueError('could not mount %s' % mount_dir)
            if output:
                logger.info("Output: " + output)
        except Exception as e:
            logger.info(e)
            raise e

    @staticmethod
    def unmount_hugetlbfs(mount_dir):
        """
        Helper function to unmount hugetlbfs
        :param mount_dir:
        :return: None if successful
        :raise Exception in case of failure
        """
        args = ["umount", mount_dir]
        try:
            process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            output, error = process.communicate()
        except:
            raise ValueError('could not umount %s' % mount_dir)
        if error != '':
            raise ValueError('could not umount %s' % mount_dir)

    @staticmethod
    def get_nr_hugepages():
        """
        Helper function to get number of hugepages
        :return: hugepages count
        :raise exception in case of failure
        """
        buf = read_linux_file("/proc/sys/vm/nr_hugepages")
        if buf is None:
            logger.info("Unable to retrieve nr_hugepages")
            raise ValueError('Unable to retrieve nr_hugepages')
        else:
            nr_hugepages = int(buf.rstrip('\r\n').strip())
        return nr_hugepages

    def setup_hugetlbfs(self):
        """
        Clean-up leftover spdk*map_* files and setup /mnt/huge if HugeTLB FS
        is not mounted by default. Set the nr_hugepage to number defined in
        the daemon configuration file.
        :return:
        """
        try:
            hugetlbfs_mount = self.get_hugetlbfs_mount()
        except Exception as e:
            logger.error('Exception in getting hugetlbfs mount', exc_info=True)
            raise e

        logger.info("hugetlbfs_mount \"%s\"" % hugetlbfs_mount)
        if hugetlbfs_mount != "":
            # If nvmf_tgt is not running then clear hugetlbfs_mount + "/spdk*map_*"
            if self.process_running == 0:
                filenames = []
                pids_not_running = []
                for (dirpath, dirnames, filenames) in os.walk(hugetlbfs_mount):
                    break
                for file in filenames:
                    m = re.match(r"^spdk_pid(\d+)map_\d+$", file)
                    if m is not None:
                        pid = int(m.group(1))
                        if pid not in pids_not_running:
                            try:
                                p = psutil.Process(pid)
                                logger.info(p.name())
                                if self.process_path in p.name():
                                    logger.info("spdk is still running?")
                                    sys.exit(-1)
                            except psutil.NoSuchProcess:
                                pids_not_running.append(pid)
                            except Exception as e:
                                logger.info(e)
                                raise
                        remove_file = (hugetlbfs_mount + '/' + file)
                        try:
                            os.remove(remove_file)
                        except:
                            pass

        # if hugetlbfs_mount == "/mnt/huge":
        #    unmount_hugetlbfs("/mnt/huge")
        #    hugetlbfs_mount = ""

        if hugetlbfs_mount == "":
            hugetlbfs_mount = "/mnt/huge"
            logger.info("Mounting HugeTLB FS at %s" % hugetlbfs_mount)
            try:
                self.mount_hugetlbfs(hugetlbfs_mount)
            except Exception as e:
                raise e
        try:
            cur_nr_hugepages = self.get_nr_hugepages()
        except Exception as e:
            raise e
        if cur_nr_hugepages < self.minimum_nr_hugepage:
            proc_hugepage_dir = "/proc/sys/vm/nr_hugepages"
            logger.info("Setting %s (%d) to (%d)" %
                        (proc_hugepage_dir,
                         cur_nr_hugepages,
                         self.minimum_nr_hugepage))
            fh = open(proc_hugepage_dir, 'wb')
            fh.write("%d\n" % self.minimum_nr_hugepage)
            fh.close()
