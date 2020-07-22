import pytest


from node_util import *


class MockDB:
    def __init__(self, Pass):
        self.Pass = Pass

    def get_key_with_prefix(self, key):
        if self.Pass:
            dict = {'cluster': {'name': 'Raven'}}
            return dict
        return None

    def get_key_value(sef, key):
        return None



def test_get_clustername_pass():
    db=MockDB(True)

    name = get_clustername(db)
    print(name)

    assert name != None
    assert name == 'Raven'


def test_get_clustername_fail():
    db=MockDB(False)

    name = get_clustername(db)
    print(name)

    assert name == None


def test_get_network_ipv4_address_fail():
    node_uuid='547654765476547698797869876987'
    mac_address='00:FF:01:02:03'

    db=MockDB(False)
    ip = get_network_ipv4_address(db, node_uuid, mac_address)

    assert ip == None
