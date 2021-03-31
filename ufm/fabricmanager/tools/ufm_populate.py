#!/usr/bin/python3
"""

   BSD LICENSE

   Copyright (c) 2021 Samsung Electronics Co., Ltd.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in
       the documentation and/or other materials provided with the
       distribution.
     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

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
