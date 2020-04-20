#!/usr/bin/python3
#
# Description:
#   The purpose of this script to be called
#   from ansible at install time
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
import etcd3 as etcd
import logging


logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

formatter = logging.Formatter('%(asctime)s:%(levelname)s:%(name)s:%(message)s')

file_handler = logging.FileHandler('ufm_populate_db.log')
file_handler.setFormatter(formatter)

logger.addHandler(file_handler)



def main():
    parser = argparse.ArgumentParser(description='Process Server\'s Configuration database.')
    parser.add_argument("--cluster_name", help="Cluster name", dest="cluster_name", default="cluster-01")
    parser.add_argument("--port", help="Port of Server", dest="port", default=2379)
    parser.add_argument("--vip_address", help="viritual ip-address", dest="vip_address", default="127.0.0.1")
    parser.add_argument("--ip_address", help="host ip-address", dest="ip_address", default="127.0.0.1")
    parser.add_argument( "--mode", help="Data Source Mode: Static, Local or DB", dest="mode", default='DB')

    args = parser.parse_args()

    hostname = str(socket.gethostname())

    logger.info("Connect to {}".format(ip_address) )

    db=etcd.client(host=ip_address, port=args.port)

    logger.info("Write default keys and values to database")

    ep = datetime.datetime(1970, 1, 1, 0, 0, 0)
    ep_sec = int((datetime.datetime.utcnow() - ep).total_seconds())

    dummy=None
    dummy = db.get('/cluster/id')
    if not dummy:
        u = str(uuid.uuid4())
        db.put('/cluster/id', u)

    dummy = db.get('/cluster/name')
    if not dummy:
        db.put('/cluster/name', cluster_name)

    dummy = db.get('/cluster/ip_address')
    if not dummy:
        db.put('/cluster/ip_address', ip_address)

    dummy = db.get('/cluster/time_created')
    if not dummy:
        db.put('/cluster/time_created', str(ep_sec))

    key="/cluster/{}/ip_address".format(hostname)
    db.put(key, vip_address)

    key="/cluster/{}/time_created".format(hostname)
    db.put(key, str(ep_sec))

    key="/cluster/{}/id".format(hostname)
    uh = str(uuid.uuid4())
    db.put(key, uh)

    logger.info("Done")


if __name__ == '__main__':
    main()
