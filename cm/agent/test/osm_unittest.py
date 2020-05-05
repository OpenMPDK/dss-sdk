import json
import os
import re
import sys
import unittest


class TestKVMethodsNegative(unittest.TestCase):
    @unittest.expectedFailure
    def test_fail_remote_list_etcd(self):
        kv_manager = kv.KVManager("0.0.0.0", "0000", )
        servers = kv_manager.backend.get_json_prefix(kv_manager.backend.ETCD_SRV_BASE)
        self.assertIsNone(servers)


class TestUtilMethods(unittest.TestCase):
    def test_flat_dict_generator(self):
        test_dict = {
            'foo4': [1],
            'foo1': 'bar1',
            'foo2': 'bar2',
            'foo3': {'bar3': 'baz3',
                     'bar4': 'baz4'},
        }
        expected = ((['foo4', 1]),
                    (['foo1', 'bar1']),
                    (['foo2', 'bar2']),
                    (['foo3', 'bar3', 'baz3']),
                    (['foo3', 'bar4', 'baz4']),)

        for idx, i in enumerate(flat_dict_generator(test_dict)):
            self.assertEquals(i, expected[idx])


class TestIdentityInfoAggregatorMethods(unittest.TestCase):
    def setUp(self):
        self.identity_obj = OSMServerIdentity()

    def runTest(self):
        identity_info = self.identity_obj.identify_server()
        self.assertTrue("Hostname" in identity_info)
        self.assertIsNot(identity_info["Hostname"], "")
        self.assertTrue("UUID" in identity_info)
        self.assertIsNotNone(re.match(r"^[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}$", identity_info["UUID"]))

    def tearDown(self):
        self.identity_obj = None


class TestCPUInfoAggregatorMethods(unittest.TestCase):
    def setUp(self):
        from server_info.server_attributes_aggregator import OSMServerCPU
        self.cpu_obj = OSMServerCPU()

    def runTest(self):
        numa_count = self.cpu_obj.get_numa_count()

        numa_map = []
        for i in range(numa_count):
            numa_map.append(self.cpu_obj.get_numa_map(i))

        cpu_info = self.cpu_obj.identify_cpu()
        self.assertTrue("PhysicalCount" in cpu_info)
        self.assertTrue(cpu_info["PhysicalCount"] > 0)
        self.assertTrue("LogicalCount" in cpu_info)
        self.assertTrue(cpu_info["LogicalCount"] > 0)
        self.assertTrue("ModelName" in cpu_info)
        self.assertTrue("NUMAMap" in cpu_info)
        for i in range(numa_count):
            self.assertEquals(numa_map[i], cpu_info["NUMAMap"][str(i)])
        self.assertTrue("Endian" in cpu_info)
        self.assertTrue(cpu_info["Endian"] == "little" or cpu_info["Endian"] == "big")
        self.assertTrue("ThreadPerCore" in cpu_info)
        self.assertTrue(cpu_info["ThreadPerCore"] > 0)
        self.assertEquals(cpu_info["PhysicalCount"] * cpu_info["ThreadPerCore"], cpu_info["LogicalCount"])
        self.assertTrue("SocketCount" in cpu_info)
        self.assertTrue(cpu_info["SocketCount"] > 0)
        self.assertTrue("NUMACount" in cpu_info)
        self.assertEquals(cpu_info["NUMACount"], len(cpu_info["NUMAMap"]))
        self.assertEquals(cpu_info["NUMACount"], numa_count)

    def tearDown(self):
        self.cpu_obj = None


class TestNetworkInfoAggregatorMethods(unittest.TestCase):
    def setUp(self):
        from server_info.server_attributes_aggregator import OSMServerNetwork
        self.network_obj = OSMServerNetwork()

    def runTest(self):
        netifaces = self.network_obj.identify_networkinterfaces()
        for iface in netifaces.values():
            self.assertTrue("MACAddress" in iface)
            self.assertIsNotNone(re.match(r"^(?:[0-9a-fA-F]{2}:?){6}$", iface["MACAddress"]))
            self.assertTrue("Vendor" in iface)
            self.assertTrue("InterfaceName" in iface)
            self.assertTrue("Duplex" in iface)
            self.assertTrue("NUMANode" in iface)
            self.assertTrue("PCIAddress" in iface)
            self.assertTrue("DriverVersion" in iface)
            self.assertTrue("Driver" in iface)
            self.assertTrue("CRC" in iface)
            self.assertTrue("IPv4" in iface)
            self.assertTrue("IPv6" in iface)
            self.assertTrue("Device" in iface)

    def tearDown(self):
        self.network_obj = None


class TestStorageInfoAggregatorMethods(unittest.TestCase):
    def setUp(self):
        from server_info.server_attributes_aggregator import OSMServerStorage
        self.storage_obj = OSMServerStorage()

    def runTest(self):
        storage_info = self.storage_obj.get_storage_db_dict()
        # print(json.dumps(storage_info, indent=2))

    def tearDown(self):
        self.storage_obj = None


if __name__ == '__main__':
    sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
    from utils.utils import flat_dict_generator
    from subcommands.kv import KVManager as kv
    from server_info.server_attributes_aggregator import OSMServerIdentity

    unittest.main()
