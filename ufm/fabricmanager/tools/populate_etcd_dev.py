#!/usr/bin/python3
#
# Description:
#   The purpose of this script is to populate DB for local development
#
import os
import os.path
import sys
import socket
import time

import argparse
import uuid
import datetime
import time
import etcd3
import logging

import yaml


logger = logging.getLogger("populate_db")
logger.setLevel(logging.DEBUG)

formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(name)s:%(message)s')

fh = logging.FileHandler('populate_db.log')
fh.setLevel(logging.ERROR)
fh.setFormatter(formatter)
logger.addHandler(fh)

ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
ch.setFormatter(formatter)
logger.addHandler(ch)


def printType(db, dbp, p, t):
    if type(t) is int or type(t) is str:
        dbp = dbp + "/" + p
        db.put(str(dbp), str(t))
        return

    if type(t) is list:
        for v in t:
            printType(db, dbp, p , v)
        return

    if type(t) is dict:
        dbp = dbp + "/" + p
        for k, v in t.items():
            printType(db, dbp, k, v)
        return


def read_data_from_file(db, filename):
    if not os.path.isfile(filename):
        logger.error("ERR: File {} doesn't exist".format(filename))
        sys.exit(-1)

    with open(filename) as fp:
        doc = yaml.load(fp)
        dbkey=''
        for key, value in doc.items():
            printType(db, dbkey, key, value)


def main():
    parser = argparse.ArgumentParser(description='Configuration db for UFM.')
    parser.add_argument("--port", help="Port of Server", dest="port", default=2379)
    parser.add_argument("--ip_address", help="ip-address of the etcd node", dest="ip_address", default="0.0.0.0")
    parser.add_argument( "--filename", help="YAML filename", dest="filename", default='cluster.yaml')

    args = parser.parse_args()

    logger.info("============> Connect to {} <=================".format(args.ip_address) )
    if args.ip_address == "0.0.0.0":
        db = etcd3.client()
    else:
        db = etcd3.client(host=args.ip_address, port=args.port)

    read_data_from_file(db, args.filename)

    logger.info("Done")


if __name__ == '__main__':
    main()
