# External imports
import json

import pytest
import requests
from flask import request, url_for

# Internal imports
from common.clusterlib import lib, lib_constants
from common.etcdlib.etcd_api import EtcdAPI

SUCCESS = 200
REST_BASE = '/redfish/v1/'
SYSTEM_ID = '4214bb34-88ab-3d8c-905c-e3772907df6f.9027bdfe-ea24-4fce-94c1-5b495d733d11'
STORAGE_ID = 'msl-ssg-mp03'


@pytest.fixture
def app():
    """Fixture to obtain Flask Server reference
    
    Returns:
        app -- flask server reference object
        By default app instance is connected to DB
    """
    from fabricmanager import app
    return app


@pytest.fixture
def client(app):
    """Fixture to create a test client 
    
    Arguments:
        app -- flask server reference object
    
    Returns:
        test_client_class -- test client
    """
    app.testing = True
    return app.test_client()


def test_redfish_service_root(client):
    response = client.get(REST_BASE)
    assert response.status_code == SUCCESS
    config = json.loads(response.data)
    assert['@odata.context', '@odata.id', '@odata.type', 'Id', 'JSONSchemas', 'Name',
           'ProtocolFeaturesSupported', 'RedfishVersion', 'Systems', 'UUID', 'Vendor'] == sorted(config.keys())
    assert config['RedfishVersion'] == '1.8.0'
    assert config['Vendor'] == 'Samsung'
    assert config['Systems'] != dict()


def test_redfish_systems(client):
    response = client.get(REST_BASE + 'Systems')
    assert response.status_code == SUCCESS
    config = json.loads(response.data)
    assert['@odata.context', '@odata.id', '@odata.type', 'Description',
           'Members', 'Members@odata.count', 'Name'] == sorted(config.keys())
    assert config['Members@odata.count'] == 8


def test_redfish_ethernet_interfaces(client):
    response = client.get(
        REST_BASE + 'Systems/' + SYSTEM_ID + '/EthernetInterfaces')
    assert response.status_code == SUCCESS
    config = json.loads(response.data)
    assert['@odata.context', '@odata.id', '@odata.type', 'Description',
           'Members', 'Members@odata.count', 'Name'] == sorted(config.keys())
    assert config['Members@odata.count'] == 2


def test_redfish_storage(client):
    response = client.get(
        REST_BASE + 'Systems/' + SYSTEM_ID + '/Storage')
    assert response.status_code == SUCCESS
    config = json.loads(response.data)
    assert['@odata.context', '@odata.id', '@odata.type', 'Description',
           'Members', 'Members@odata.count', 'Name', 'oem'] == sorted(config.keys())
    assert config['Members@odata.count'] == 1


def test_redfish_drives(client):
    response = client.get(
        REST_BASE + 'Systems/' + SYSTEM_ID + '/Storage/' + STORAGE_ID + '/Drives')
    assert response.status_code == SUCCESS
    config = json.loads(response.data)
    assert['@odata.context', '@odata.id', '@odata.type', 'Description', 'Id',
           'Members', 'Members@odata.count', 'Name'] == sorted(config.keys())
    assert config['Members@odata.count'] == 4
