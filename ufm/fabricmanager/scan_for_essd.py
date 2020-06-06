#!/usr/bin/python3
#
# Description:
#   This is just a simple tool to scan fake essd and
#   enter then into the database
#
import os
import os.path
import sys
import requests
import argparse
import json
import yaml

# from common.clusterlib import lib
from common.ufmdb import ufmdb

from systems.essd import essd_constants


# If application is installed, then append the module search path
if os.path.dirname(os.path.realpath(__file__)) == "/usr/share/ufm":
    sys.path.append("/usr/share/ufm/")


def readConfigDataFromFile(filename):
    configData = dict()
    if os.path.isfile(filename):
        with open(filename) as f:
            configData = yaml.load(f)
    else:
        print("ERR: Failed to open yaml file with essdUrls")
        sys.exit(-1)

    return configData


def insertEssdUrlsToDb(db, key, essdUrls):
    listOfDrives = list()
    try:
        tmpString = db.get(key).decode('utf-8')

        if tmpString:
            listOfDrives = json.loads(tmpString)
    except:
        pass

    for d in essdUrls:
        if d not in listOfDrives:
            listOfDrives.append(d)

    jsonString = json.dumps(listOfDrives, indent=4, sort_keys=True)
    db.put(key, jsonString)


class DbTest:
    def __init__(self):
        pass

    def __del__(self):
        pass

    def get(self, key):
        return None

    def put(self, key, value):
        pass
        print("key={} value={}".format(key, value))


    def get_key_value(self, key):
        return None

    def save_key_value(self, key, value):
        pass
        print("key={} value={}".format(key, value))


def main():
    parser = argparse.ArgumentParser(description='Utility to insert essd into etdd database.')
    parser.add_argument("--ip_address", help="host ip-address", dest="ip_address", default="127.0.0.1")
    parser.add_argument("--port", help="Port of Server", dest="port", default=2379)
    parser.add_argument( "--nodes", help="Nodes with fake essds", dest="nodes", default='asimceph1.autocache.com' )
    parser.add_argument( "--filename", help="YAML filename", dest="filename", default='nodes.yaml')
    parser.add_argument("--fromfile", help="Read essd Urls from file", dest="fromfile", default=0)

    args = parser.parse_args()

    try:
        # db = lib.create_db_connection(ip_address=args.ip_address)
        # db = DbTest()
        db = ufmdb.client(db_type = 'etcd')
    except:
        print("ERR: Failed to connect to DB.")
        sys.exit(-1)

    essdsUrls=list()

    # read_data_from_file(args.filename)
    if args.fromfile != 0:
        essdsUrls = readConfigDataFromFile(args.filename)
        insertEssdUrlsToDb(db, essd_constants.ESSDURLS_KEY, essdsUrls)
        return

    # nodes="asimceph1.autocache.com,asimceph2.autocache.com,asimceph3.autocache.com,asimceph4.autocache.com"
    # for node in nodes.split(','):

    for node in args.nodes.split(','):
        requestCommand="http://{}:11000/services".format(node)

        print("{}".format(requestCommand) )

        try:
            listOfEssds = requests.get(requestCommand)
        except:
            continue

        try:
            essds=listOfEssds.json()['services']
        except:
            continue

        for essd in essds:
            essdsUrls.append("http://{}:{}".format(essd['ip_addr'], essd['port'] ))

    insertEssdUrlsToDb(db, essd_constants.ESSDURLS_KEY, essdsUrls)


if __name__ == '__main__':
    main()
